# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
import threading
import time

import amqp
import pika
import pika.adapters.blocking_connection
import pika.exceptions
import pika.spec
import pytest

from typing import Optional

from hs5_fixture import Hs5Runner, hs5_amqp


def connect(
    runner: Hs5Runner, password=None, heartbeat=None
) -> pika.BlockingConnection:
    params = pika.ConnectionParameters(
        host="127.0.0.1",
        port=runner.get_amqp_port(),
        credentials=pika.PlainCredentials("root", password or runner.get_root_key()),
        heartbeat=heartbeat,
    )
    return pika.BlockingConnection(params)


Channel = pika.adapters.blocking_connection.BlockingChannel

# BlockingConnection is deprecated in recent pika versions, but still the common API
pytestmark = pytest.mark.filterwarnings(
    "ignore:BlockingConnection is deprecated:DeprecationWarning"
)


def get_body(ch: Channel, queue: str, auto_ack=True) -> Optional[bytes]:
    method, props, body = ch.basic_get(queue, auto_ack=auto_ack)
    return body


def get_msg(
    ch: Channel, queue: str, auto_ack=False
) -> tuple[pika.spec.Basic.GetOk, pika.spec.BasicProperties, bytes]:
    """basic.get of a message which has to exist"""
    method, props, body = ch.basic_get(queue, auto_ack=auto_ack)
    assert method is not None and props is not None and body is not None
    return method, props, body


def tag(method) -> int:
    assert method.delivery_tag is not None
    return method.delivery_tag


def test_amqp_basic(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    res = ch.queue_declare("q1", durable=True)
    assert res.method.queue == "q1"
    assert res.method.message_count == 0

    props = pika.BasicProperties(
        content_type="text/plain",
        headers={"a": 1},
        delivery_mode=2,
        correlation_id="c1",
    )
    ch.basic_publish("", "q1", b"hello", props)
    ch.basic_publish("", "q1", b"world")
    assert ch.queue_declare("q1", passive=True).method.message_count == 2

    method, p, body = get_msg(ch, "q1")
    assert body == b"hello"
    assert p.content_type == "text/plain"
    assert p.headers == {"a": 1}
    assert p.correlation_id == "c1"
    assert method.redelivered is False
    assert method.message_count == 1
    ch.basic_ack(tag(method))

    # Requeue via nack: redelivered
    method, p, body = get_msg(ch, "q1")
    assert body == b"world"
    ch.basic_nack(tag(method), requeue=True)
    method, p, body = get_msg(ch, "q1")
    assert body == b"world"
    assert method.redelivered is True
    ch.basic_reject(tag(method), requeue=False)
    assert ch.basic_get("q1") == (None, None, None)

    # Consumer
    for i in range(10):
        ch.basic_publish("", "q1", str(i).encode())
    received = []

    def on_message(ch2, method, props, body):
        received.append(body)
        ch2.basic_ack(tag(method))
        if len(received) == 10:
            ch2.stop_consuming()

    ch.basic_qos(prefetch_count=3)
    ch.basic_consume("q1", on_message)
    ch.start_consuming()
    assert received == [str(i).encode() for i in range(10)]

    # Large message (several frames)
    big = bytes(range(256)) * 4096
    ch.basic_publish("", "q1", big)
    assert get_body(ch, "q1") == big

    # Unknown queue
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.queue_declare("missing", passive=True)
    assert e.value.reply_code == 404
    conn.close()


def test_amqp_exchanges(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("direct1", "direct")
    ch.exchange_declare("fanout1", "fanout")
    ch.exchange_declare("topic1", "topic")
    for q in ["d1", "d2", "f1", "f2", "t1", "t2", "t3"]:
        ch.queue_declare(q)
    ch.queue_bind("d1", "direct1", "rk1")
    ch.queue_bind("d2", "direct1", "rk2")
    ch.queue_bind("f1", "fanout1")
    ch.queue_bind("f2", "fanout1")
    ch.queue_bind("t1", "topic1", "a.*")
    ch.queue_bind("t2", "topic1", "a.#")
    ch.queue_bind("t3", "topic1", "#.c")

    ch.basic_publish("direct1", "rk1", b"x")
    ch.basic_publish("fanout1", "ignored", b"y")
    ch.basic_publish("topic1", "a.b", b"ab")
    ch.basic_publish("topic1", "a.b.c", b"abc")
    ch.basic_publish("topic1", "a", b"a")

    assert get_body(ch, "d1") == b"x"
    assert get_body(ch, "d2") is None
    assert get_body(ch, "f1") == b"y"
    assert get_body(ch, "f2") == b"y"
    assert get_body(ch, "t1") == b"ab"
    assert get_body(ch, "t1") is None
    assert [get_body(ch, "t2") for _ in range(4)] == [b"ab", b"abc", b"a", None]
    assert get_body(ch, "t3") == b"abc"

    # Unbind
    ch.queue_unbind("d1", "direct1", "rk1")
    ch.basic_publish("direct1", "rk1", b"x")
    assert get_body(ch, "d1") is None

    # Mandatory, unroutable: returned
    returned = []
    ch.add_on_return_callback(
        lambda c, method, props, body: returned.append((method.reply_code, body))
    )
    ch.basic_publish("direct1", "nothing", b"ret", mandatory=True)
    conn.process_data_events(time_limit=1)
    assert returned == [(312, b"ret")]

    # Unknown exchange
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.basic_publish("missing", "x", b"x")
        ch.queue_declare("d1", passive=True)
    assert e.value.reply_code == 404

    ch = conn.channel()
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.exchange_declare("direct1", "fanout")
    assert e.value.reply_code == 406

    ch = conn.channel()
    ch.queue_purge("f1")
    for _ in range(3):
        ch.basic_publish("", "f1", b"p")
    assert ch.queue_purge("f1").method.message_count == 3
    ch.basic_publish("", "f1", b"p")
    assert ch.queue_delete("f1").method.message_count == 1
    with pytest.raises(pika.exceptions.ChannelClosedByBroker):
        ch.queue_declare("f1", passive=True)
    conn.close()


def test_amqp_confirms_and_redelivery(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.confirm_delivery()
    ch.queue_declare("c1", durable=True)
    for i in range(20):
        ch.basic_publish("", "c1", str(i).encode())
    assert ch.queue_declare("c1", passive=True).method.message_count == 20

    # Unacked messages of a closed connection are requeued (redelivered)
    conn2 = connect(hs5_amqp)
    ch2 = conn2.channel()
    method, props, body = get_msg(ch2, "c1")
    assert body == b"0"
    conn2.close()
    method, props, body = get_msg(ch, "c1")
    assert body == b"0"
    assert method.redelivered is True
    ch.basic_ack(tag(method))

    # Prefetch: a consumer only gets prefetch_count unacked messages
    ch2 = connect(hs5_amqp).channel()
    ch2.basic_qos(prefetch_count=2)
    got = []
    ch2.basic_consume("c1", lambda c, m, p, b: got.append(m.delivery_tag))

    def wait_for(n: int):
        deadline = time.monotonic() + 5
        while len(got) < n and time.monotonic() < deadline:
            ch2.connection.process_data_events(time_limit=0.1)
        # No more than n
        ch2.connection.process_data_events(time_limit=0.5)

    wait_for(2)
    assert len(got) == 2
    ch2.basic_ack(got[1], multiple=True)
    wait_for(4)
    assert len(got) == 4
    ch2.connection.close()
    assert ch.queue_declare("c1", passive=True).method.message_count == 17
    conn.close()


def test_amqp_persistence(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("dex", "direct", durable=True)
    ch.queue_declare("dq", durable=True)
    ch.queue_bind("dq", "dex", "k")
    ch.queue_declare("tq", durable=False)
    for i in range(3):
        ch.queue_declare(f"tq{i}", durable=False)
    ch.confirm_delivery()
    for i in range(5):
        ch.basic_publish(
            "dex", "k", f"m{i}".encode(), pika.BasicProperties(delivery_mode=2)
        )
    ch.basic_publish("", "tq", b"transient")
    for i in range(3):
        for j in range(4):
            ch.basic_publish("", f"tq{i}", f"t{i}-{j}".encode())
    method, props, body = get_msg(ch, "dq")
    assert body == b"m0"
    ch.basic_ack(tag(method))
    conn.close()

    # Acks are committed lazily (by the maintenance thread, every 500 ms). restart() kills the server
    time.sleep(1.5)
    hs5_amqp.commit_storage(hs5_amqp.get_s3_client())
    hs5_amqp.restart()

    conn = connect(hs5_amqp)
    ch = conn.channel()
    assert ch.queue_declare("dq", passive=True).method.message_count == 4
    with pytest.raises(pika.exceptions.ChannelClosedByBroker):
        ch.queue_declare("tq", passive=True)
    ch = conn.channel()
    # Messages of the non-durable queues were deleted (only the durable queue's key is left)
    hs5_amqp.commit_storage(hs5_amqp.get_s3_client())
    s3 = hs5_amqp.get_s3_client()
    queue_keys = {
        o.get("Key", "")
        for o in s3.list_objects_v2(Bucket=hs5_amqp.amqp_bucket(), Prefix="q/").get(
            "Contents", []
        )
    }
    assert len(queue_keys) == 1
    for i in range(3):
        assert ch.queue_declare(f"tq{i}").method.message_count == 0
        assert get_body(ch, f"tq{i}") is None
    # Binding was stored
    ch.basic_publish("dex", "k", b"after")
    bodies = []
    while True:
        method, props, body = ch.basic_get("dq", auto_ack=True)
        if method is None or body is None:
            break
        # Messages from before the restart may have been delivered
        assert method.redelivered == (body != b"after")
        bodies.append(body)
    assert bodies == [b"m1", b"m2", b"m3", b"m4", b"after"]

    # Messages are objects in the bucket
    s3 = hs5_amqp.get_s3_client()
    keys = [
        o.get("Key", "")
        for o in s3.list_objects_v2(Bucket=hs5_amqp.amqp_bucket()).get("Contents", [])
    ]
    assert any(k.startswith("$m/q/") for k in keys)
    conn.close()


def test_amqp_lazy_commit(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("lazy", durable=True)
    ch.confirm_delivery()
    # Confirmed without a commit (non-persistent), persistent ones after a commit
    ch.basic_publish("", "lazy", b"transient")
    ch.basic_publish("", "lazy", b"persistent", pika.BasicProperties(delivery_mode=2))
    ch.basic_publish("", "lazy", b"transient2")

    # Listings only include committed objects. The maintenance thread commits the non-persistent changes
    s3 = hs5_amqp.get_s3_client()
    ch.queue_declare("lazy2")
    ch.basic_publish("", "lazy2", b"transient3")
    queue_keys: set[str] = set()
    for _ in range(50):
        queue_keys = {
            o.get("Key", "")
            for o in s3.list_objects_v2(Bucket=hs5_amqp.amqp_bucket(), Prefix="q/").get(
                "Contents", []
            )
        }
        if len(queue_keys) == 2:
            break
        time.sleep(0.1)
    assert len(queue_keys) == 2

    bodies = []
    for _ in range(3):
        method, props, body = get_msg(ch, "lazy")
        bodies.append(body)
        ch.basic_ack(tag(method))
    assert bodies == [b"transient", b"persistent", b"transient2"]
    assert get_body(ch, "lazy2") == b"transient3"
    assert get_body(ch, "lazy2") is None
    conn.close()


def test_amqp_confirmed_durable(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("cd", durable=True)
    ch.confirm_delivery()
    for i in range(20):
        ch.basic_publish(
            "", "cd", f"c{i}".encode(), pika.BasicProperties(delivery_mode=2)
        )
    conn.close()

    # Confirmed persistent messages survive a crash (restart() kills the server)
    hs5_amqp.restart()
    conn = connect(hs5_amqp)
    ch = conn.channel()
    assert ch.queue_declare("cd", passive=True).method.message_count == 20
    bodies = [get_body(ch, "cd") for _ in range(20)]
    assert bodies == [f"c{i}".encode() for i in range(20)]
    conn.close()


def test_amqp_ttl_and_expiry(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("ttl", arguments={"x-message-ttl": 200})
    ch.basic_publish("", "ttl", b"old")
    time.sleep(0.4)
    ch.basic_publish("", "ttl", b"new")
    # The expired message is dropped
    assert get_body(ch, "ttl") == b"new"
    assert get_body(ch, "ttl") is None
    ch.queue_declare("ttl2")
    ch.basic_publish("", "ttl2", b"e", pika.BasicProperties(expiration="100"))
    ch.basic_publish("", "ttl2", b"keep")
    time.sleep(0.3)
    assert get_body(ch, "ttl2") == b"keep"

    ch.queue_declare("exp", arguments={"x-expires": 500})
    time.sleep(1.5)
    with pytest.raises(pika.exceptions.ChannelClosedByBroker):
        ch.queue_declare("exp", passive=True)
    conn.close()


def test_amqp_exclusive_and_auth(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    name = ch.queue_declare("", exclusive=True).method.queue
    assert isinstance(name, str) and name.startswith("amq.gen-")

    conn2 = connect(hs5_amqp)
    ch2 = conn2.channel()
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch2.queue_declare(name, passive=True)
    assert e.value.reply_code == 405
    conn.close()
    # Exclusive queues are deleted with their connection
    deadline = time.monotonic() + 5
    while True:
        ch2 = conn2.channel()
        try:
            ch2.queue_declare(name, passive=True)
        except pika.exceptions.ChannelClosedByBroker as err:
            assert err.reply_code == 404
            break
        assert time.monotonic() < deadline
        time.sleep(0.1)
    conn2.close()

    with pytest.raises(pika.exceptions.ProbableAuthenticationError):
        connect(hs5_amqp, password="wrong")


def py_amqp_publish(ch: amqp.Channel, msg: amqp.Message, routing_key: str):
    # basic_publish is assigned per instance in py-amqp (confirm mode), which type checkers don't understand
    ch.basic_publish(msg, routing_key=routing_key)  # pyright: ignore[reportCallIssue]


def test_amqp_py_amqp(hs5_amqp: Hs5Runner):
    """py-amqp (used by Celery/kombu), with AMQPLAIN login and heartbeats"""
    with amqp.Connection(
        f"127.0.0.1:{hs5_amqp.get_amqp_port()}",
        userid="root",
        password=hs5_amqp.get_root_key(),
        heartbeat=2,
    ) as conn:
        ch = conn.channel()
        ch.queue_declare("pa", auto_delete=False)
        py_amqp_publish(ch, amqp.Message("hi", content_type="text/plain"), "pa")
        received = []

        def cb(msg):
            received.append(msg.body)
            ch.basic_ack(msg.delivery_tag)

        ch.basic_consume("pa", callback=cb)
        conn.drain_events(timeout=5)
        assert received == ["hi"]

        # Heartbeats keep the idle connection open
        for _ in range(6):
            conn.heartbeat_tick()
            time.sleep(0.5)
        py_amqp_publish(ch, amqp.Message("again"), "pa")
        conn.drain_events(timeout=5)
        assert received == ["hi", "again"]


def test_amqp_concurrent(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("work")
    total = 400
    consumed: list[bytes] = []
    lock = threading.Lock()

    def consumer():
        c = connect(hs5_amqp)
        cch = c.channel()
        cch.basic_qos(prefetch_count=5)

        def on_message(ch2, method, props, body):
            with lock:
                consumed.append(body)
            ch2.basic_ack(tag(method))

        cch.basic_consume("work", on_message)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            c.process_data_events(time_limit=0.2)
            with lock:
                if len(consumed) >= total:
                    break
        c.close()

    def producer(start):
        c = connect(hs5_amqp)
        pch = c.channel()
        pch.confirm_delivery()
        for i in range(start, start + total // 4):
            pch.basic_publish("", "work", str(i).encode())
        c.close()

    threads = [threading.Thread(target=consumer) for _ in range(3)]
    threads += [
        threading.Thread(target=producer, args=(i * total // 4,)) for i in range(4)
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert sorted(int(b) for b in consumed) == list(range(total))
    assert ch.queue_declare("work", passive=True).method.message_count == 0
    conn.close()


def test_amqp_headers_exchange(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("hdr", "headers")
    for q in ["h_all", "h_any", "h_present"]:
        ch.queue_declare(q)
    ch.queue_bind(
        "h_all", "hdr", arguments={"x-match": "all", "format": "pdf", "type": "report"}
    )
    ch.queue_bind(
        "h_any", "hdr", arguments={"x-match": "any", "format": "pdf", "type": "log"}
    )
    # Void value: header only has to be present
    ch.queue_bind("h_present", "hdr", arguments={"x-match": "all", "tag": None})

    def pub(body: bytes, headers: dict):
        ch.basic_publish("hdr", "", body, pika.BasicProperties(headers=headers))

    pub(b"1", {"format": "pdf", "type": "report"})
    pub(b"2", {"format": "pdf", "type": "other"})
    pub(b"3", {"type": "log", "tag": "x"})
    pub(b"4", {"format": "zip"})

    assert [get_body(ch, "h_all") for _ in range(2)] == [b"1", None]
    assert [get_body(ch, "h_any") for _ in range(4)] == [b"1", b"2", b"3", None]
    assert [get_body(ch, "h_present") for _ in range(2)] == [b"3", None]
    conn.close()


def test_amqp_exchange_to_exchange(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("src", "topic", durable=True)
    ch.exchange_declare("mid", "fanout", durable=True)
    ch.exchange_declare("dst", "direct", durable=True)
    ch.queue_declare("e2e_mid", durable=True)
    ch.queue_declare("e2e_dst", durable=True)
    ch.exchange_bind(destination="mid", source="src", routing_key="a.#")
    ch.exchange_bind(destination="dst", source="mid")
    # Cycle
    ch.exchange_bind(destination="src", source="dst", routing_key="k")
    ch.queue_bind("e2e_mid", "mid")
    ch.queue_bind("e2e_dst", "dst", "a.b")

    ch.basic_publish("src", "a.b", b"ab")
    ch.basic_publish("src", "a.c", b"ac")
    ch.basic_publish("src", "x", b"none")
    assert [get_body(ch, "e2e_mid") for _ in range(3)] == [b"ab", b"ac", None]
    # Delivered once despite the cycle
    assert [get_body(ch, "e2e_dst") for _ in range(2)] == [b"ab", None]

    ch.exchange_unbind(destination="dst", source="mid")
    ch.basic_publish("src", "a.b", b"ab2")
    assert get_body(ch, "e2e_dst") is None
    assert get_body(ch, "e2e_mid") == b"ab2"
    ch.exchange_bind(destination="dst", source="mid")
    conn.close()

    # Exchange bindings are stored
    hs5_amqp.commit_storage(hs5_amqp.get_s3_client())
    hs5_amqp.restart()
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.basic_publish("src", "a.b", b"after")
    assert get_body(ch, "e2e_mid") == b"after"
    assert get_body(ch, "e2e_dst") == b"after"

    # Deleting an exchange removes the bindings to it
    ch.exchange_delete("mid")
    ch.basic_publish("src", "a.b", b"gone")
    assert get_body(ch, "e2e_dst") is None
    conn.close()


def test_amqp_transactions(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("txq")
    ch.tx_select()
    ch.basic_publish("", "txq", b"a")
    ch.basic_publish("", "txq", b"b")

    other = conn.channel()
    assert other.queue_declare("txq", passive=True).method.message_count == 0
    ch.tx_commit()
    assert other.queue_declare("txq", passive=True).method.message_count == 2

    ch.basic_publish("", "txq", b"rolled back")
    ch.tx_rollback()
    assert other.queue_declare("txq", passive=True).method.message_count == 2

    # Acks are applied on commit
    method, props, body = get_msg(ch, "txq")
    assert body == b"a"
    ch.basic_ack(tag(method))
    ch.tx_rollback()
    # Not acked: requeued when the channel is closed
    ch.close()
    assert other.queue_declare("txq", passive=True).method.message_count == 2

    ch = conn.channel()
    ch.tx_select()
    method, props, body = get_msg(ch, "txq")
    ch.basic_ack(tag(method))
    ch.tx_commit()
    ch.close()
    assert other.queue_declare("txq", passive=True).method.message_count == 1

    # tx and confirm modes are exclusive
    ch = conn.channel()
    ch.tx_select()
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.confirm_delivery()
    assert e.value.reply_code == 406
    ch = conn.channel()
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.tx_commit()
    assert e.value.reply_code == 406
    conn.close()


def test_amqp_dead_lettering(hs5_amqp: Hs5Runner):
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("dlx", "direct")
    ch.queue_declare("dead")
    ch.queue_bind("dead", "dlx", "dead-rk")
    ch.queue_declare(
        "work_dl",
        arguments={
            "x-dead-letter-exchange": "dlx",
            "x-dead-letter-routing-key": "dead-rk",
        },
    )
    ch.queue_declare(
        "ttl_dl",
        arguments={
            "x-dead-letter-exchange": "dlx",
            "x-dead-letter-routing-key": "dead-rk",
            "x-message-ttl": 100,
        },
    )

    # Rejected (without requeue)
    ch.basic_publish(
        "", "work_dl", b"rejected", pika.BasicProperties(headers={"h": "v"})
    )
    method, props, body = get_msg(ch, "work_dl")
    ch.basic_reject(tag(method), requeue=False)
    method, props, body = get_msg(ch, "dead", auto_ack=True)
    assert body == b"rejected"
    assert method.routing_key == "dead-rk"
    assert props.headers is not None
    assert props.headers["h"] == "v"
    death = props.headers["x-death"][0]
    assert death["reason"] == "rejected"
    assert death["queue"] == "work_dl"
    assert death["count"] == 1
    assert death["routing-keys"] == ["work_dl"]
    assert death["exchange"] == ""
    assert props.headers["x-first-death-reason"] == "rejected"

    # Nacked without requeue
    ch.basic_publish("", "work_dl", b"nacked")
    method, props, body = get_msg(ch, "work_dl")
    ch.basic_nack(tag(method), requeue=False)
    assert get_body(ch, "dead") == b"nacked"

    # Expired (no consumer): dead lettered by the server, without the expiration
    ch.basic_publish("", "ttl_dl", b"expired")
    ch.basic_publish("", "work_dl", b"expiring", pika.BasicProperties(expiration="100"))
    deadline = time.monotonic() + 5
    got = []
    while len(got) < 2 and time.monotonic() < deadline:
        method, props, body = ch.basic_get("dead", auto_ack=True)
        if body is None:
            time.sleep(0.1)
            continue
        assert props is not None and props.headers is not None
        assert props.headers["x-death"][0]["reason"] == "expired"
        assert props.expiration is None
        got.append(body)
    assert sorted(got) == [b"expired", b"expiring"]
    assert get_body(ch, "ttl_dl") is None

    # Requeued messages are not dead lettered
    ch.basic_publish("", "work_dl", b"requeued")
    method, props, body = get_msg(ch, "work_dl")
    ch.basic_reject(tag(method), requeue=True)
    assert get_body(ch, "dead") is None
    assert get_body(ch, "work_dl") == b"requeued"

    # Dead lettered twice: count is incremented
    ch.queue_declare(
        "loop_dl",
        arguments={
            "x-dead-letter-exchange": "",
            "x-dead-letter-routing-key": "loop_dl",
        },
    )
    ch.basic_publish("", "loop_dl", b"loop")
    for expected_count in [1, 2]:
        method, props, body = get_msg(ch, "loop_dl")
        ch.basic_reject(tag(method), requeue=False)
    method, props, body = get_msg(ch, "loop_dl", auto_ack=True)
    assert props.headers is not None
    assert props.headers["x-death"][0]["count"] == 2
    conn.close()


def test_amqp_large_messages(hs5_amqp: Hs5Runner):
    """Large bodies are streamed to and from the data file"""
    import os

    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.confirm_delivery()
    ch.exchange_declare("big_fanout", "fanout")
    ch.queue_declare("big1")
    ch.queue_declare("big2")
    ch.queue_bind("big1", "big_fanout")
    ch.queue_bind("big2", "big_fanout")

    bodies = [
        os.urandom(5 * 1024 * 1024 + 17),
        b"small",
        os.urandom(200 * 1024),
        b"",
        os.urandom(3 * 1024 * 1024),
    ]
    for body in bodies:
        ch.basic_publish("big_fanout", "", body)

    # Interleaved with deliveries on another channel of the same connection
    ch2 = conn.channel()
    received: dict[str, list[bytes]] = {"big1": [], "big2": []}

    def make_cb(name: str):
        def cb(c, method, props, body):
            received[name].append(body)
            c.basic_ack(tag(method))

        return cb

    ch.basic_consume("big1", make_cb("big1"))
    ch2.basic_consume("big2", make_cb("big2"))
    deadline = time.monotonic() + 60
    while (
        len(received["big1"]) < len(bodies) or len(received["big2"]) < len(bodies)
    ) and time.monotonic() < deadline:
        conn.process_data_events(time_limit=0.2)
    assert received["big1"] == bodies
    assert received["big2"] == bodies

    # basic.get and transactions with a large body
    big = os.urandom(2 * 1024 * 1024)
    ch3 = conn.channel()
    ch3.queue_declare("big3")
    ch3.tx_select()
    ch3.basic_publish("", "big3", big)
    ch3.tx_rollback()
    ch3.basic_publish("", "big3", big)
    ch3.tx_commit()
    assert ch3.queue_declare("big3", passive=True).method.message_count == 1
    assert get_body(ch3, "big3") == big
    conn.close()

    # Too large
    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.queue_declare("big4")
    with pytest.raises(pika.exceptions.ChannelClosedByBroker) as e:
        ch.basic_publish("", "big4", b"x" * (128 * 1024 * 1024 + 1))
        ch.queue_declare("big4", passive=True)
    assert e.value.reply_code == 311
    conn.close()


def test_amqp_dead_lettering_large(hs5_amqp: Hs5Runner):
    """Large dead lettered messages are copied in chunks (to several queues)"""
    import os

    conn = connect(hs5_amqp)
    ch = conn.channel()
    ch.exchange_declare("dlx_big", "fanout")
    ch.queue_declare("dead_big1")
    ch.queue_declare("dead_big2")
    ch.queue_bind("dead_big1", "dlx_big")
    ch.queue_bind("dead_big2", "dlx_big")
    ch.queue_declare("work_big", arguments={"x-dead-letter-exchange": "dlx_big"})
    ch.queue_declare(
        "ttl_big", arguments={"x-dead-letter-exchange": "dlx_big", "x-message-ttl": 100}
    )

    rejected = os.urandom(3 * 1024 * 1024 + 5)
    ch.basic_publish("", "work_big", rejected)
    method, props, body = get_msg(ch, "work_big")
    assert body == rejected
    ch.basic_reject(tag(method), requeue=False)
    assert get_body(ch, "dead_big1") == rejected
    assert get_body(ch, "dead_big2") == rejected

    expired = os.urandom(2 * 1024 * 1024)
    ch.basic_publish("", "ttl_big", expired)
    deadline = time.monotonic() + 5
    body1 = None
    while body1 is None and time.monotonic() < deadline:
        body1 = get_body(ch, "dead_big1")
        time.sleep(0.1)
    assert body1 == expired
    assert get_body(ch, "dead_big2") == expired
    conn.close()
