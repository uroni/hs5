# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Runs Celery (with the Redis broker and result backend) against the Redis protocol server
"""
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterator, Optional

import pytest

celery = pytest.importorskip("celery")

from celery import Celery, chain, chord, group

import uuid

from hs5_fixture import Hs5Runner

TASKS_MODULE = """
import os
import time
from pathlib import Path

from celery import Celery

app = Celery("celery_tasks", broker=os.environ["HS5_REDIS_URL"], backend=os.environ["HS5_REDIS_URL"])
app.conf.update(
    broker_transport_options={
        "visibility_timeout": int(os.environ.get("VISIBILITY_TIMEOUT", "3600")),
        "unacked_restore_interval": 1,
        "unacked_restore_throttle": 1,
        # A killed worker may hold the restore mutex. Let it expire quickly
        "unacked_mutex_expire": 5,
    },
    result_expires=300,
    worker_prefetch_multiplier=1,
)


@app.task
def add(x, y):
    return x + y


@app.task
def tsum(values):
    return sum(values)


@app.task
def fail():
    raise ValueError("boom")


@app.task(bind=True, max_retries=5, default_retry_delay=0.1)
def flaky(self, marker_dir):
    attempts = len(os.listdir(marker_dir))
    Path(marker_dir, str(attempts)).touch()
    if attempts < 2:
        raise self.retry()
    return attempts


@app.task(acks_late=True)
def slow(marker, seconds):
    Path(marker).write_text(str(os.getpid()))
    time.sleep(seconds)
    return "slow done"
"""


def redis_url(runner: Hs5Runner) -> str:
    password = runner.get_redis_client().connection_pool.connection_kwargs["password"]
    return f"redis://root:{password}@127.0.0.1:{runner.get_redis_port()}/0"


class Worker:
    def __init__(
        self, workdir: Path, url: str, name: str, extra_env: dict[str, str] = {}
    ):
        env = dict(os.environ)
        env["HS5_REDIS_URL"] = url
        env["PYTHONPATH"] = str(workdir)
        env.update(extra_env)
        self.log = open(workdir / f"{name}.log", "wb")
        self.process = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "celery",
                "-A",
                "celery_tasks",
                "worker",
                "--pool=prefork",
                "--concurrency=2",
                "--loglevel=INFO",
                f"--hostname={name}@%h",
                "--without-heartbeat",
            ],
            cwd=workdir,
            env=env,
            stdout=self.log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        self.name = name

    def kill(self):
        if self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGKILL)
            self.process.wait()

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                self.kill()
        self.log.close()


def wait_for_worker(app: Celery, name: str, timeout: float = 60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        # Broadcast via pub/sub (pidbox fanout exchange), replies via a list
        replies = app.control.inspect(timeout=1).ping() or {}
        if any(k.startswith(f"{name}@") for k in replies):
            return
    raise TimeoutError(f"Celery worker {name} did not start")


@pytest.fixture(params=["inline", "large"])
def celery_redis(request, tmp_path: Path, testrun_uid: str) -> Iterator[Hs5Runner]:
    """Redis server. With "large", hashes/sets/sorted sets (e.g. the unacked messages) are stored with one entry per element"""
    loc = tmp_path / uuid.uuid4().hex
    loc.mkdir()
    extra_args = []
    if request.param == "large":
        extra_args.append("--redis_collection_inline_max_entries=0")
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=100,
        testrun_uid=testrun_uid,
        with_redis=True,
        extra_args=extra_args,
    )
    yield runner
    runner.stop(cleanup=True, kill=False)


@pytest.fixture
def celery_env(
    celery_redis: Hs5Runner, tmp_path: Path
) -> Iterator[tuple[Celery, Path, str]]:
    (tmp_path / "celery_tasks.py").write_text(TASKS_MODULE)
    sys.path.insert(0, str(tmp_path))
    url = redis_url(celery_redis)
    os.environ["HS5_REDIS_URL"] = url
    sys.modules.pop("celery_tasks", None)
    import celery_tasks  # type: ignore

    try:
        yield celery_tasks.app, tmp_path, url
    finally:
        celery_tasks.app.close()
        sys.path.remove(str(tmp_path))
        sys.modules.pop("celery_tasks", None)


def print_log(workdir: Path, name: str):
    log = workdir / f"{name}.log"
    if log.exists():
        print(log.read_text(errors="replace")[-20000:])


def test_celery_tasks(celery_redis: Hs5Runner, celery_env: tuple[Celery, Path, str]):
    app, workdir, url = celery_env
    import celery_tasks  # type: ignore

    worker = Worker(workdir, url, "w1")
    try:
        wait_for_worker(app, "w1")

        assert celery_tasks.add.delay(2, 3).get(timeout=30) == 5

        res = group(celery_tasks.add.s(i, i) for i in range(10))()
        assert res.get(timeout=30) == [i * 2 for i in range(10)]

        # Chords use Lua scripts in the result backend
        res = chord(celery_tasks.add.s(i, i) for i in range(5))(celery_tasks.tsum.s())
        assert res.get(timeout=30) == 20

        res = chain(celery_tasks.add.s(1, 2), celery_tasks.add.s(3))()
        assert res.get(timeout=30) == 6

        with pytest.raises(ValueError, match="boom"):
            celery_tasks.fail.delay().get(timeout=30)

        marker_dir = workdir / "flaky"
        marker_dir.mkdir()
        assert celery_tasks.flaky.delay(str(marker_dir)).get(timeout=30) == 2

        # ETA tasks are held by the worker (unacked hash/sorted set)
        start = time.monotonic()
        assert celery_tasks.add.apply_async((1, 1), countdown=2).get(timeout=30) == 2
        assert time.monotonic() - start >= 1.5

        # Results expire (result_expires)
        res = celery_tasks.add.delay(5, 5)
        assert res.get(timeout=30) == 10
        r = celery_redis.get_redis_client()
        ttl = r.ttl(f"celery-task-meta-{res.id}")
        assert 0 < ttl <= 300

        results = [celery_tasks.add.delay(i, 1) for i in range(100)]
        assert [res.get(timeout=60) for res in results] == [i + 1 for i in range(100)]

        stats = app.control.inspect(timeout=5).stats()
        assert stats and any(k.startswith("w1@") for k in stats)
    except Exception:
        print_log(workdir, "w1")
        raise
    finally:
        worker.stop()


def test_celery_redelivery(
    celery_redis: Hs5Runner, celery_env: tuple[Celery, Path, str]
):
    """An acks_late task of a killed worker is redelivered after the visibility timeout"""
    app, workdir, url = celery_env
    import celery_tasks  # type: ignore

    env = {"VISIBILITY_TIMEOUT": "3"}
    worker1: Optional[Worker] = Worker(workdir, url, "w1", env)
    worker2: Optional[Worker] = None
    try:
        wait_for_worker(app, "w1")

        marker = workdir / "slow-marker"
        res = celery_tasks.slow.delay(str(marker), 5)
        deadline = time.monotonic() + 30
        while not marker.exists() and time.monotonic() < deadline:
            time.sleep(0.1)
        assert marker.exists()
        first_pid = marker.read_text()

        worker1.kill()
        marker.unlink()

        worker2 = Worker(workdir, url, "w2", env)
        assert res.get(timeout=90) == "slow done"
        assert marker.exists() and marker.read_text() != first_pid
    except Exception:
        print_log(workdir, "w1")
        print_log(workdir, "w2")
        raise
    finally:
        if worker1:
            worker1.stop()
        if worker2:
            worker2.stop()
