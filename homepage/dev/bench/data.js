window.BENCHMARK_DATA = {
  "lastUpdate": 1751222025462,
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
          "id": "91926cf7be8ddd78a1ac32ddd7ac2b8cb56d0de2",
          "message": "Reuse free space in data file if there is some of it",
          "timestamp": "2025-05-11T20:43:39+02:00",
          "tree_id": "b870b88ce7d10b8eef91ca8677145ccca9c34d48",
          "url": "https://github.com/uroni/hs5/commit/91926cf7be8ddd78a1ac32ddd7ac2b8cb56d0de2"
        },
        "date": 1746991331450,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0688413100598756,
            "unit": "iter/sec",
            "range": "stddev: 0.10414746992307422",
            "extra": "mean: 14.52616167719989 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06262753942069026,
            "unit": "iter/sec",
            "range": "stddev: 0.2649537991041208",
            "extra": "mean: 15.967416399400008 sec\nrounds: 5"
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
          "id": "ae7db50109f27136a1089c6852b0a56666feafac",
          "message": "Make test more robust w.r.t. alloc chunk size changes",
          "timestamp": "2025-05-11T20:54:43+02:00",
          "tree_id": "a694e1bf33a443d10d2f815c0b29d2c62066ff83",
          "url": "https://github.com/uroni/hs5/commit/ae7db50109f27136a1089c6852b0a56666feafac"
        },
        "date": 1746991907752,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07604469313700958,
            "unit": "iter/sec",
            "range": "stddev: 0.05831420706623104",
            "extra": "mean: 13.15016155300018 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0678049159002567,
            "unit": "iter/sec",
            "range": "stddev: 0.0728855986830536",
            "extra": "mean: 14.748193205799907 sec\nrounds: 5"
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
          "id": "ae7db50109f27136a1089c6852b0a56666feafac",
          "message": "Make test more robust w.r.t. alloc chunk size changes",
          "timestamp": "2025-05-11T20:54:43+02:00",
          "tree_id": "a694e1bf33a443d10d2f815c0b29d2c62066ff83",
          "url": "https://github.com/uroni/hs5/commit/ae7db50109f27136a1089c6852b0a56666feafac"
        },
        "date": 1747169984815,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0745903486772582,
            "unit": "iter/sec",
            "range": "stddev: 0.07446306594608727",
            "extra": "mean: 13.40656020160004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06696499572888874,
            "unit": "iter/sec",
            "range": "stddev: 0.12330704994726016",
            "extra": "mean: 14.933174998600043 sec\nrounds: 5"
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
          "id": "0b4f3323e45de06359d47bd60b4d0c78967806f5",
          "message": "Fix build and assertion",
          "timestamp": "2025-06-29T16:29:47+02:00",
          "tree_id": "50c35a0a0d293a16892fa1ab4a855df4be697506",
          "url": "https://github.com/uroni/hs5/commit/0b4f3323e45de06359d47bd60b4d0c78967806f5"
        },
        "date": 1751210738462,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07843688354660404,
            "unit": "iter/sec",
            "range": "stddev: 0.104648943921019",
            "extra": "mean: 12.749104181400071 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06847955081874578,
            "unit": "iter/sec",
            "range": "stddev: 0.19924604488621242",
            "extra": "mean: 14.602899523199813 sec\nrounds: 5"
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
          "id": "1aeaa77facec537ef53d1c7583d9fe59fff00fa0",
          "message": "Increment version",
          "timestamp": "2025-06-29T16:31:03+02:00",
          "tree_id": "b93092420a54544b56c5030a2ef36e8bbf574cb5",
          "url": "https://github.com/uroni/hs5/commit/1aeaa77facec537ef53d1c7583d9fe59fff00fa0"
        },
        "date": 1751210758927,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07473661530921588,
            "unit": "iter/sec",
            "range": "stddev: 0.032573457999266205",
            "extra": "mean: 13.380322293999962 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06453117269327821,
            "unit": "iter/sec",
            "range": "stddev: 0.24031320320012423",
            "extra": "mean: 15.496386603000065 sec\nrounds: 5"
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
          "id": "1aeaa77facec537ef53d1c7583d9fe59fff00fa0",
          "message": "Increment version",
          "timestamp": "2025-06-29T16:31:03+02:00",
          "tree_id": "b93092420a54544b56c5030a2ef36e8bbf574cb5",
          "url": "https://github.com/uroni/hs5/commit/1aeaa77facec537ef53d1c7583d9fe59fff00fa0"
        },
        "date": 1751210831490,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07689085011176804,
            "unit": "iter/sec",
            "range": "stddev: 0.08364105303181975",
            "extra": "mean: 13.005448613800036 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06813347581981596,
            "unit": "iter/sec",
            "range": "stddev: 0.1149543854315682",
            "extra": "mean: 14.677073024199945 sec\nrounds: 5"
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
          "id": "b0c7b988f71876631b50b4680678cda750b9e403",
          "message": "Add timeout to tests",
          "timestamp": "2025-06-29T19:32:53+02:00",
          "tree_id": "c419c95db8b571373d2bb367c5032345794b9bd6",
          "url": "https://github.com/uroni/hs5/commit/b0c7b988f71876631b50b4680678cda750b9e403"
        },
        "date": 1751222024461,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07085876813338744,
            "unit": "iter/sec",
            "range": "stddev: 0.17816048475124963",
            "extra": "mean: 14.112579520399777 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06484042689042471,
            "unit": "iter/sec",
            "range": "stddev: 0.1600908209089439",
            "extra": "mean: 15.422477117399648 sec\nrounds: 5"
          }
        ]
      }
    ]
  }
}