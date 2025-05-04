window.BENCHMARK_DATA = {
  "lastUpdate": 1746398356252,
  "repoUrl": "https://github.com/uroni/hs5",
  "entries": {
    "Benchmark": [
      {
        "commit": {
          "author": {
            "email": "martin@urbackup.org",
            "name": "Martin Raiber",
            "username": "uroni"
          },
          "committer": {
            "email": "martin@urbackup.org",
            "name": "Martin Raiber",
            "username": "uroni"
          },
          "distinct": true,
          "id": "7129909246792f1b3a6a530f1b7f8511b04123e1",
          "message": "Improve commit performance",
          "timestamp": "2025-05-04T23:48:00+02:00",
          "tree_id": "bfa29c9df1082158282f07c964787f68d749b75c",
          "url": "https://github.com/uroni/hs5/commit/7129909246792f1b3a6a530f1b7f8511b04123e1"
        },
        "date": 1746397554912,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0730477306011396,
            "unit": "iter/sec",
            "range": "stddev: 0.10953841181884895",
            "extra": "mean: 13.689679224400152 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06548658915412799,
            "unit": "iter/sec",
            "range": "stddev: 0.2264400145604347",
            "extra": "mean: 15.270302101800098 sec\nrounds: 5"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "martin@urbackup.org",
            "name": "Martin Raiber",
            "username": "uroni"
          },
          "committer": {
            "email": "martin@urbackup.org",
            "name": "Martin Raiber",
            "username": "uroni"
          },
          "distinct": true,
          "id": "364b7e833c20d14e3416a335d0f9614b805a22b2",
          "message": "Try using manual vcpkg caching",
          "timestamp": "2025-05-05T00:01:25+02:00",
          "tree_id": "7a77875fa1a94763ce161713ac2e0f72a9cd2863",
          "url": "https://github.com/uroni/hs5/commit/364b7e833c20d14e3416a335d0f9614b805a22b2"
        },
        "date": 1746398355268,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07435389517137607,
            "unit": "iter/sec",
            "range": "stddev: 0.07085191981997827",
            "extra": "mean: 13.449194526999964 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06628964366690474,
            "unit": "iter/sec",
            "range": "stddev: 0.18277268756793666",
            "extra": "mean: 15.085312647400041 sec\nrounds: 5"
          }
        ]
      }
    ]
  }
}