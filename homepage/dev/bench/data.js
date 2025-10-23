window.BENCHMARK_DATA = {
  "lastUpdate": 1761256441444,
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
        "date": 1751225641676,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0716527278663092,
            "unit": "iter/sec",
            "range": "stddev: 0.16274978904993656",
            "extra": "mean: 13.956202782199943 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06459628313932096,
            "unit": "iter/sec",
            "range": "stddev: 0.30363302044365426",
            "extra": "mean: 15.480766870799744 sec\nrounds: 5"
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
          "id": "93f4bf77b76326d754966825077c96e63c34940a",
          "message": "Experiment with using webdav cache",
          "timestamp": "2025-06-29T21:09:34+02:00",
          "tree_id": "171c0536ccae883c59d211829cbddd298d57c07e",
          "url": "https://github.com/uroni/hs5/commit/93f4bf77b76326d754966825077c96e63c34940a"
        },
        "date": 1751227583255,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07372496378124153,
            "unit": "iter/sec",
            "range": "stddev: 0.038855533961155875",
            "extra": "mean: 13.563926636399902 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0658643240825538,
            "unit": "iter/sec",
            "range": "stddev: 0.09705657963610681",
            "extra": "mean: 15.182726216799983 sec\nrounds: 5"
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
          "id": "ef8ef1ef5e442354a065ef5c0ac22cd0c4cf6051",
          "message": "Experiment with using webdav cache",
          "timestamp": "2025-06-29T21:11:32+02:00",
          "tree_id": "3b73cfd9c28a75f45e8a5f03427e3198778546df",
          "url": "https://github.com/uroni/hs5/commit/ef8ef1ef5e442354a065ef5c0ac22cd0c4cf6051"
        },
        "date": 1751227626110,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07026142291946187,
            "unit": "iter/sec",
            "range": "stddev: 0.26587117129827703",
            "extra": "mean: 14.232561175800038 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06299162178542295,
            "unit": "iter/sec",
            "range": "stddev: 0.3060893261460906",
            "extra": "mean: 15.875127067000085 sec\nrounds: 5"
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
          "id": "c3aeb8e89803c5b4d898a2d0ac4547c43ea0f023",
          "message": "Experiment with using webdav cache",
          "timestamp": "2025-06-29T21:12:50+02:00",
          "tree_id": "46d424f02d7535464e1194827dde0035d257aa6e",
          "url": "https://github.com/uroni/hs5/commit/c3aeb8e89803c5b4d898a2d0ac4547c43ea0f023"
        },
        "date": 1751227708638,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07701349013372266,
            "unit": "iter/sec",
            "range": "stddev: 0.05043880539192337",
            "extra": "mean: 12.984738105799988 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0674237284767179,
            "unit": "iter/sec",
            "range": "stddev: 0.07341552375309043",
            "extra": "mean: 14.831573729199954 sec\nrounds: 5"
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
          "id": "0caf533d8d30e5b384fd35d9156eb4ac431684de",
          "message": "Experiment with using webdav cache",
          "timestamp": "2025-06-29T21:18:05+02:00",
          "tree_id": "39d1f6a2fcbbbd4de05ebe0a2d8ff6fcdf7952db",
          "url": "https://github.com/uroni/hs5/commit/0caf533d8d30e5b384fd35d9156eb4ac431684de"
        },
        "date": 1751228043001,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07398906929839265,
            "unit": "iter/sec",
            "range": "stddev: 0.06067637102553665",
            "extra": "mean: 13.515509918999943 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06461230977440943,
            "unit": "iter/sec",
            "range": "stddev: 0.1885970043115809",
            "extra": "mean: 15.47692697399998 sec\nrounds: 5"
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
          "id": "8a57d756f2863a932293019ca881f6f88045affc",
          "message": "Write to vcpkg cache",
          "timestamp": "2025-06-29T23:22:55+02:00",
          "tree_id": "c487b66a0f7ec070928fb6786850fc5a71abc5f3",
          "url": "https://github.com/uroni/hs5/commit/8a57d756f2863a932293019ca881f6f88045affc"
        },
        "date": 1751235526007,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06896570447423035,
            "unit": "iter/sec",
            "range": "stddev: 0.055055188731160214",
            "extra": "mean: 14.49996063439994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06292044666737737,
            "unit": "iter/sec",
            "range": "stddev: 0.2790589140486993",
            "extra": "mean: 15.893084886799988 sec\nrounds: 5"
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
          "id": "8a57d756f2863a932293019ca881f6f88045affc",
          "message": "Write to vcpkg cache",
          "timestamp": "2025-06-29T23:22:55+02:00",
          "tree_id": "c487b66a0f7ec070928fb6786850fc5a71abc5f3",
          "url": "https://github.com/uroni/hs5/commit/8a57d756f2863a932293019ca881f6f88045affc"
        },
        "date": 1751236849341,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0727666646897163,
            "unit": "iter/sec",
            "range": "stddev: 0.796989555668452",
            "extra": "mean: 13.742556488800073 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06376499707462872,
            "unit": "iter/sec",
            "range": "stddev: 0.8272373771810126",
            "extra": "mean: 15.682585209399894 sec\nrounds: 5"
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
          "id": "0aadfd43a7b05b7b2b076d1f445d7574c3edf54d",
          "message": "Add clean step",
          "timestamp": "2025-06-30T01:46:15+02:00",
          "tree_id": "0201a32fde83f1a225a9c491e21829687b70c74f",
          "url": "https://github.com/uroni/hs5/commit/0aadfd43a7b05b7b2b076d1f445d7574c3edf54d"
        },
        "date": 1751241196225,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07725007343671642,
            "unit": "iter/sec",
            "range": "stddev: 0.03557362928738618",
            "extra": "mean: 12.944971512799976 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06789318320172254,
            "unit": "iter/sec",
            "range": "stddev: 0.20097893634606018",
            "extra": "mean: 14.729019215799985 sec\nrounds: 5"
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
          "id": "6bbb28bcb0de4d7e5142ce1d04ddf86dc92a96a1",
          "message": "Disable really_min_space",
          "timestamp": "2025-06-30T01:50:45+02:00",
          "tree_id": "f79f7294361c47e01a06fcabe0a4ab4e628a575d",
          "url": "https://github.com/uroni/hs5/commit/6bbb28bcb0de4d7e5142ce1d04ddf86dc92a96a1"
        },
        "date": 1751241478946,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07127030167632267,
            "unit": "iter/sec",
            "range": "stddev: 0.27233278148315915",
            "extra": "mean: 14.031089759400004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06475074479555855,
            "unit": "iter/sec",
            "range": "stddev: 0.19007461403140427",
            "extra": "mean: 15.443837799200002 sec\nrounds: 5"
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
          "id": "6bbb28bcb0de4d7e5142ce1d04ddf86dc92a96a1",
          "message": "Disable really_min_space",
          "timestamp": "2025-06-30T01:50:45+02:00",
          "tree_id": "f79f7294361c47e01a06fcabe0a4ab4e628a575d",
          "url": "https://github.com/uroni/hs5/commit/6bbb28bcb0de4d7e5142ce1d04ddf86dc92a96a1"
        },
        "date": 1751241541377,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07226524029686869,
            "unit": "iter/sec",
            "range": "stddev: 0.0390060426521032",
            "extra": "mean: 13.837911503399937 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06370156368609942,
            "unit": "iter/sec",
            "range": "stddev: 0.13027360340636596",
            "extra": "mean: 15.698201773000028 sec\nrounds: 5"
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
          "id": "ab2b76af91bd708f7d6b62a09b1dccce0be80029",
          "message": "Fix multi-part object deletion",
          "timestamp": "2025-07-03T22:21:04+02:00",
          "tree_id": "d441f7565f46aebe9214b61502011a1cfc99abaa",
          "url": "https://github.com/uroni/hs5/commit/ab2b76af91bd708f7d6b62a09b1dccce0be80029"
        },
        "date": 1751574511065,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07369584668920763,
            "unit": "iter/sec",
            "range": "stddev: 0.07977279415773451",
            "extra": "mean: 13.569285718599996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06553871670304956,
            "unit": "iter/sec",
            "range": "stddev: 0.27741013688665833",
            "extra": "mean: 15.258156556999984 sec\nrounds: 5"
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
          "id": "ab2b76af91bd708f7d6b62a09b1dccce0be80029",
          "message": "Fix multi-part object deletion",
          "timestamp": "2025-07-03T22:21:04+02:00",
          "tree_id": "d441f7565f46aebe9214b61502011a1cfc99abaa",
          "url": "https://github.com/uroni/hs5/commit/ab2b76af91bd708f7d6b62a09b1dccce0be80029"
        },
        "date": 1751574581500,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06871650554093307,
            "unit": "iter/sec",
            "range": "stddev: 0.36742393069290946",
            "extra": "mean: 14.552544430600005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06512331888612105,
            "unit": "iter/sec",
            "range": "stddev: 0.19213119277505905",
            "extra": "mean: 15.355482753399997 sec\nrounds: 5"
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
          "id": "15b8770bf2621f38f0dc2fb2defaac2f16f6f9d9",
          "message": "Install packages for vcpkg",
          "timestamp": "2025-07-12T14:37:17+02:00",
          "tree_id": "d5fc95c24ccce76f1eb610dc5b5ef3545b0ddbe2",
          "url": "https://github.com/uroni/hs5/commit/15b8770bf2621f38f0dc2fb2defaac2f16f6f9d9"
        },
        "date": 1752326505130,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07243917179619881,
            "unit": "iter/sec",
            "range": "stddev: 0.051737415062720664",
            "extra": "mean: 13.804685713599975 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0654013699922489,
            "unit": "iter/sec",
            "range": "stddev: 0.2179074784851308",
            "extra": "mean: 15.29019958020017 sec\nrounds: 5"
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
          "id": "15b8770bf2621f38f0dc2fb2defaac2f16f6f9d9",
          "message": "Install packages for vcpkg",
          "timestamp": "2025-07-12T14:37:17+02:00",
          "tree_id": "d5fc95c24ccce76f1eb610dc5b5ef3545b0ddbe2",
          "url": "https://github.com/uroni/hs5/commit/15b8770bf2621f38f0dc2fb2defaac2f16f6f9d9"
        },
        "date": 1752326959981,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0714590153497165,
            "unit": "iter/sec",
            "range": "stddev: 0.1977368866980432",
            "extra": "mean: 13.994035533599993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06373675839480818,
            "unit": "iter/sec",
            "range": "stddev: 0.38271789297531755",
            "extra": "mean: 15.689533405600013 sec\nrounds: 5"
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
          "id": "3882ea3fd6e14e4b92f82dd6107ea06032b4cd80",
          "message": "Fix duckdb static linking",
          "timestamp": "2025-07-12T16:43:30+02:00",
          "tree_id": "c22dba5d088bc57c6f46a7e85afab1505f517044",
          "url": "https://github.com/uroni/hs5/commit/3882ea3fd6e14e4b92f82dd6107ea06032b4cd80"
        },
        "date": 1752333697446,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07286761689573773,
            "unit": "iter/sec",
            "range": "stddev: 0.12510741211549534",
            "extra": "mean: 13.72351728520016 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06427986016843426,
            "unit": "iter/sec",
            "range": "stddev: 0.14069603062949304",
            "extra": "mean: 15.556972236399906 sec\nrounds: 5"
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
          "id": "9f6d9926b2a37b366735fbf1de2b6684c595be6d",
          "message": "Add empty listing test",
          "timestamp": "2025-07-12T17:44:03+02:00",
          "tree_id": "c951cdf9665e10542c609c358c817bdcf3a96606",
          "url": "https://github.com/uroni/hs5/commit/9f6d9926b2a37b366735fbf1de2b6684c595be6d"
        },
        "date": 1752335553807,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06977176481192149,
            "unit": "iter/sec",
            "range": "stddev: 0.0633444753127686",
            "extra": "mean: 14.332445261999965 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06268870759988877,
            "unit": "iter/sec",
            "range": "stddev: 0.15923721817986086",
            "extra": "mean: 15.951836276199993 sec\nrounds: 5"
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
          "id": "9f6d9926b2a37b366735fbf1de2b6684c595be6d",
          "message": "Add empty listing test",
          "timestamp": "2025-07-12T17:44:03+02:00",
          "tree_id": "c951cdf9665e10542c609c358c817bdcf3a96606",
          "url": "https://github.com/uroni/hs5/commit/9f6d9926b2a37b366735fbf1de2b6684c595be6d"
        },
        "date": 1752335567207,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07360499798192395,
            "unit": "iter/sec",
            "range": "stddev: 0.16792960515838137",
            "extra": "mean: 13.586033929999996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0628946749052732,
            "unit": "iter/sec",
            "range": "stddev: 0.2731111973826492",
            "extra": "mean: 15.899597247400004 sec\nrounds: 5"
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
          "id": "76c54af71cc99d361446f7045a65c114a50cd1a4",
          "message": "Properly remove trailing slash from bucket name",
          "timestamp": "2025-07-12T17:58:56+02:00",
          "tree_id": "656d9ccfb4310d56d2f6b79eec1bd2bbe70664f1",
          "url": "https://github.com/uroni/hs5/commit/76c54af71cc99d361446f7045a65c114a50cd1a4"
        },
        "date": 1752336401425,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06965762208955156,
            "unit": "iter/sec",
            "range": "stddev: 0.06717651425249288",
            "extra": "mean: 14.3559307654 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06267000257997433,
            "unit": "iter/sec",
            "range": "stddev: 0.27158681160862813",
            "extra": "mean: 15.956597396400003 sec\nrounds: 5"
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
          "id": "76c54af71cc99d361446f7045a65c114a50cd1a4",
          "message": "Properly remove trailing slash from bucket name",
          "timestamp": "2025-07-12T17:58:56+02:00",
          "tree_id": "656d9ccfb4310d56d2f6b79eec1bd2bbe70664f1",
          "url": "https://github.com/uroni/hs5/commit/76c54af71cc99d361446f7045a65c114a50cd1a4"
        },
        "date": 1752336424200,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07271477077043843,
            "unit": "iter/sec",
            "range": "stddev: 0.04877009057021725",
            "extra": "mean: 13.752364057599994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06544127548839643,
            "unit": "iter/sec",
            "range": "stddev: 0.1589158371419519",
            "extra": "mean: 15.280875755199986 sec\nrounds: 5"
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
          "id": "0f01fbe10854c285cef73e15f88cbe10d65a731a",
          "message": "Support x-id PUT param",
          "timestamp": "2025-07-12T18:57:41+02:00",
          "tree_id": "5836e518c3b56c504069db8211c55c17f380fe55",
          "url": "https://github.com/uroni/hs5/commit/0f01fbe10854c285cef73e15f88cbe10d65a731a"
        },
        "date": 1752339919265,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0704858067559918,
            "unit": "iter/sec",
            "range": "stddev: 0.11121222202403946",
            "extra": "mean: 14.187253378000003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06425860150903051,
            "unit": "iter/sec",
            "range": "stddev: 0.24286631793979135",
            "extra": "mean: 15.562118946200007 sec\nrounds: 5"
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
          "id": "0f01fbe10854c285cef73e15f88cbe10d65a731a",
          "message": "Support x-id PUT param",
          "timestamp": "2025-07-12T18:57:41+02:00",
          "tree_id": "5836e518c3b56c504069db8211c55c17f380fe55",
          "url": "https://github.com/uroni/hs5/commit/0f01fbe10854c285cef73e15f88cbe10d65a731a"
        },
        "date": 1752339931445,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07478323604756318,
            "unit": "iter/sec",
            "range": "stddev: 0.10861194331137514",
            "extra": "mean: 13.371980845600024 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06578085014712268,
            "unit": "iter/sec",
            "range": "stddev: 0.18210868685389037",
            "extra": "mean: 15.201992643199992 sec\nrounds: 5"
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
          "id": "891065224aaadaf4329d72c0682dfee3a8b7316b",
          "message": "Fix returned etag",
          "timestamp": "2025-07-12T19:51:11+02:00",
          "tree_id": "32dee06144e172947ed1bee1dfb04067ab501631",
          "url": "https://github.com/uroni/hs5/commit/891065224aaadaf4329d72c0682dfee3a8b7316b"
        },
        "date": 1752343155716,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07020440327300913,
            "unit": "iter/sec",
            "range": "stddev: 0.21703207267243407",
            "extra": "mean: 14.244120786999996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06418895486975143,
            "unit": "iter/sec",
            "range": "stddev: 0.2506434705728252",
            "extra": "mean: 15.579004238800007 sec\nrounds: 5"
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
          "id": "891065224aaadaf4329d72c0682dfee3a8b7316b",
          "message": "Fix returned etag",
          "timestamp": "2025-07-12T19:51:11+02:00",
          "tree_id": "32dee06144e172947ed1bee1dfb04067ab501631",
          "url": "https://github.com/uroni/hs5/commit/891065224aaadaf4329d72c0682dfee3a8b7316b"
        },
        "date": 1752343173220,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07145917019216366,
            "unit": "iter/sec",
            "range": "stddev: 0.12683439869374108",
            "extra": "mean: 13.994005210400019 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06541579848230253,
            "unit": "iter/sec",
            "range": "stddev: 0.21022035064233693",
            "extra": "mean: 15.28682708460003 sec\nrounds: 5"
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
          "id": "d30ac45b312dafbdc62faa45cb6c68e9c9af3673",
          "message": "Put correct last modified into list response",
          "timestamp": "2025-07-12T20:09:48+02:00",
          "tree_id": "88dde654912f7030ac776fdb35ce6fbc4aeda841",
          "url": "https://github.com/uroni/hs5/commit/d30ac45b312dafbdc62faa45cb6c68e9c9af3673"
        },
        "date": 1752344330208,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06966297732331528,
            "unit": "iter/sec",
            "range": "stddev: 0.08805781406813706",
            "extra": "mean: 14.354827175399999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06292488770430117,
            "unit": "iter/sec",
            "range": "stddev: 0.16006027217279112",
            "extra": "mean: 15.89196320379998 sec\nrounds: 5"
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
          "id": "d30ac45b312dafbdc62faa45cb6c68e9c9af3673",
          "message": "Put correct last modified into list response",
          "timestamp": "2025-07-12T20:09:48+02:00",
          "tree_id": "88dde654912f7030ac776fdb35ce6fbc4aeda841",
          "url": "https://github.com/uroni/hs5/commit/d30ac45b312dafbdc62faa45cb6c68e9c9af3673"
        },
        "date": 1752344477117,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07043083478282261,
            "unit": "iter/sec",
            "range": "stddev: 0.11465120101759564",
            "extra": "mean: 14.198326671599954 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06400112879067345,
            "unit": "iter/sec",
            "range": "stddev: 0.3105396906261521",
            "extra": "mean: 15.624724421199971 sec\nrounds: 5"
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
          "id": "d339f54c8c3423b3a897ba6cbe3ebfa0209913d0",
          "message": "Use bool to signal extent initialization\n\nSo it also works with zero length files",
          "timestamp": "2025-07-13T23:00:24+02:00",
          "tree_id": "30f8cee9885d4c2d0efa465fac2d3024e4c86883",
          "url": "https://github.com/uroni/hs5/commit/d339f54c8c3423b3a897ba6cbe3ebfa0209913d0"
        },
        "date": 1752440877830,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07718887221120463,
            "unit": "iter/sec",
            "range": "stddev: 0.15377638124400878",
            "extra": "mean: 12.955235273599987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06832657182722918,
            "unit": "iter/sec",
            "range": "stddev: 0.22554632983625725",
            "extra": "mean: 14.63559451699997 sec\nrounds: 5"
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
          "id": "d339f54c8c3423b3a897ba6cbe3ebfa0209913d0",
          "message": "Use bool to signal extent initialization\n\nSo it also works with zero length files",
          "timestamp": "2025-07-13T23:00:24+02:00",
          "tree_id": "30f8cee9885d4c2d0efa465fac2d3024e4c86883",
          "url": "https://github.com/uroni/hs5/commit/d339f54c8c3423b3a897ba6cbe3ebfa0209913d0"
        },
        "date": 1752440948357,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07276647796458009,
            "unit": "iter/sec",
            "range": "stddev: 0.17707305854176034",
            "extra": "mean: 13.742591753399983 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06431941144037857,
            "unit": "iter/sec",
            "range": "stddev: 0.23829162980657673",
            "extra": "mean: 15.547405947999982 sec\nrounds: 5"
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
          "id": "f7145583a1ee6452487b715d3fdbdd5bffbcae75",
          "message": "Fix default commit after ms",
          "timestamp": "2025-07-14T21:41:18+02:00",
          "tree_id": "cf4bdeffe19bd574bafd409b8fab32be77795f99",
          "url": "https://github.com/uroni/hs5/commit/f7145583a1ee6452487b715d3fdbdd5bffbcae75"
        },
        "date": 1752522592500,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06519054889148353,
            "unit": "iter/sec",
            "range": "stddev: 0.46596452480946876",
            "extra": "mean: 15.339646881400006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05488554578781383,
            "unit": "iter/sec",
            "range": "stddev: 0.23476015789017873",
            "extra": "mean: 18.2197331856 sec\nrounds: 5"
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
          "id": "f7145583a1ee6452487b715d3fdbdd5bffbcae75",
          "message": "Fix default commit after ms",
          "timestamp": "2025-07-14T21:41:18+02:00",
          "tree_id": "cf4bdeffe19bd574bafd409b8fab32be77795f99",
          "url": "https://github.com/uroni/hs5/commit/f7145583a1ee6452487b715d3fdbdd5bffbcae75"
        },
        "date": 1752522611033,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07479899099189616,
            "unit": "iter/sec",
            "range": "stddev: 0.09315038001129212",
            "extra": "mean: 13.369164299399994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06647518426908115,
            "unit": "iter/sec",
            "range": "stddev: 0.173076081093226",
            "extra": "mean: 15.043207642000004 sec\nrounds: 5"
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
          "id": "b6130769f3766fc47c4db6d86325984289886b02",
          "message": "Return correct etag when listing",
          "timestamp": "2025-07-17T01:42:15+02:00",
          "tree_id": "efb8afb0376b8fc9299e87d37abf410cec3aee45",
          "url": "https://github.com/uroni/hs5/commit/b6130769f3766fc47c4db6d86325984289886b02"
        },
        "date": 1752709796702,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07808606541297722,
            "unit": "iter/sec",
            "range": "stddev: 0.0745149910714635",
            "extra": "mean: 12.806382223399988 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0686044075277349,
            "unit": "iter/sec",
            "range": "stddev: 0.17570180934974955",
            "extra": "mean: 14.576322951199995 sec\nrounds: 5"
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
          "id": "b6130769f3766fc47c4db6d86325984289886b02",
          "message": "Return correct etag when listing",
          "timestamp": "2025-07-17T01:42:15+02:00",
          "tree_id": "efb8afb0376b8fc9299e87d37abf410cec3aee45",
          "url": "https://github.com/uroni/hs5/commit/b6130769f3766fc47c4db6d86325984289886b02"
        },
        "date": 1752709808833,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07771691906271813,
            "unit": "iter/sec",
            "range": "stddev: 0.12310622199776693",
            "extra": "mean: 12.867211053400002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06815850445848316,
            "unit": "iter/sec",
            "range": "stddev: 0.24578945749061157",
            "extra": "mean: 14.671683423000013 sec\nrounds: 5"
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
          "id": "a4794eb71a9c11ddc653a3faeeb9c581e7581b45",
          "message": "Fix build",
          "timestamp": "2025-09-28T18:10:37+02:00",
          "tree_id": "7f76c53c5903baa3c7d8065280ed87c62a892aed",
          "url": "https://github.com/uroni/hs5/commit/a4794eb71a9c11ddc653a3faeeb9c581e7581b45"
        },
        "date": 1759076352378,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07160414279508681,
            "unit": "iter/sec",
            "range": "stddev: 0.22431199863011636",
            "extra": "mean: 13.965672389400016 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05757751726231782,
            "unit": "iter/sec",
            "range": "stddev: 1.6216834961770188",
            "extra": "mean: 17.36789023819997 sec\nrounds: 5"
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
          "id": "a4794eb71a9c11ddc653a3faeeb9c581e7581b45",
          "message": "Fix build",
          "timestamp": "2025-09-28T18:10:37+02:00",
          "tree_id": "7f76c53c5903baa3c7d8065280ed87c62a892aed",
          "url": "https://github.com/uroni/hs5/commit/a4794eb71a9c11ddc653a3faeeb9c581e7581b45"
        },
        "date": 1759076798649,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07944287671500799,
            "unit": "iter/sec",
            "range": "stddev: 0.07632842868501141",
            "extra": "mean: 12.587660988000005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06892861836293913,
            "unit": "iter/sec",
            "range": "stddev: 0.11118669630098892",
            "extra": "mean: 14.507762142199999 sec\nrounds: 5"
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
          "id": "c88ccb84973073859876744d6ed9d4ceecc9168a",
          "message": "Handle content sha256 being set to UNSIGNED-PAYLOAD",
          "timestamp": "2025-09-28T20:00:03+02:00",
          "tree_id": "6f1019ad246db4ddc4d5ea176bc3f8d9d24eb5f7",
          "url": "https://github.com/uroni/hs5/commit/c88ccb84973073859876744d6ed9d4ceecc9168a"
        },
        "date": 1759084598528,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0747315450278305,
            "unit": "iter/sec",
            "range": "stddev: 0.0775893672781636",
            "extra": "mean: 13.381230103400025 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06647253435242528,
            "unit": "iter/sec",
            "range": "stddev: 0.13431304413772274",
            "extra": "mean: 15.043807336999999 sec\nrounds: 5"
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
          "id": "c88ccb84973073859876744d6ed9d4ceecc9168a",
          "message": "Handle content sha256 being set to UNSIGNED-PAYLOAD",
          "timestamp": "2025-09-28T20:00:03+02:00",
          "tree_id": "6f1019ad246db4ddc4d5ea176bc3f8d9d24eb5f7",
          "url": "https://github.com/uroni/hs5/commit/c88ccb84973073859876744d6ed9d4ceecc9168a"
        },
        "date": 1759084625362,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07575531125111667,
            "unit": "iter/sec",
            "range": "stddev: 0.027819436602108907",
            "extra": "mean: 13.200394579399994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06682973590749079,
            "unit": "iter/sec",
            "range": "stddev: 0.2043253505578897",
            "extra": "mean: 14.963398948400037 sec\nrounds: 5"
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
          "id": "0b2ad7ca83b4821ee5d31f50bdadb3a6a90ecd21",
          "message": "Also go to multi-part upload path if uploads parameter is set",
          "timestamp": "2025-09-28T21:07:46+02:00",
          "tree_id": "500b9a4c04a438fefbbb0c7b56074266bce3fb1e",
          "url": "https://github.com/uroni/hs5/commit/0b2ad7ca83b4821ee5d31f50bdadb3a6a90ecd21"
        },
        "date": 1759086965766,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07569745277814947,
            "unit": "iter/sec",
            "range": "stddev: 0.23551784459857675",
            "extra": "mean: 13.210484148399985 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06607690789971207,
            "unit": "iter/sec",
            "range": "stddev: 0.32046136426182065",
            "extra": "mean: 15.133880076799983 sec\nrounds: 5"
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
          "id": "0b2ad7ca83b4821ee5d31f50bdadb3a6a90ecd21",
          "message": "Also go to multi-part upload path if uploads parameter is set",
          "timestamp": "2025-09-28T21:07:46+02:00",
          "tree_id": "500b9a4c04a438fefbbb0c7b56074266bce3fb1e",
          "url": "https://github.com/uroni/hs5/commit/0b2ad7ca83b4821ee5d31f50bdadb3a6a90ecd21"
        },
        "date": 1759087032008,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07419901879443995,
            "unit": "iter/sec",
            "range": "stddev: 0.08873762778873896",
            "extra": "mean: 13.477267169400012 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06638389755905448,
            "unit": "iter/sec",
            "range": "stddev: 0.10495578483382517",
            "extra": "mean: 15.063894058200026 sec\nrounds: 5"
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
          "id": "32f068c62fd53e2f2d3ea3458be2a672f216894d",
          "message": "Test for Last-Modified being returned",
          "timestamp": "2025-10-05T12:18:50+02:00",
          "tree_id": "a264f8a8a1c0b56445b6b4858c7d7c469ddadd66",
          "url": "https://github.com/uroni/hs5/commit/32f068c62fd53e2f2d3ea3458be2a672f216894d"
        },
        "date": 1759660063137,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07346890852382901,
            "unit": "iter/sec",
            "range": "stddev: 0.14221775312403862",
            "extra": "mean: 13.611199895200002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.062110291910448374,
            "unit": "iter/sec",
            "range": "stddev: 0.23840082939917556",
            "extra": "mean: 16.100391243399987 sec\nrounds: 5"
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
          "id": "4c7db4b4158e50e7c4116d8eeec8da8e3e89ea29",
          "message": "Update version",
          "timestamp": "2025-10-05T12:20:22+02:00",
          "tree_id": "f685e8c429205551dd3e17836320ac222b2e6b11",
          "url": "https://github.com/uroni/hs5/commit/4c7db4b4158e50e7c4116d8eeec8da8e3e89ea29"
        },
        "date": 1759660118799,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07398865959897878,
            "unit": "iter/sec",
            "range": "stddev: 0.08224271768781649",
            "extra": "mean: 13.515584758800014 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0653245243058315,
            "unit": "iter/sec",
            "range": "stddev: 0.1911196878136866",
            "extra": "mean: 15.308186483200007 sec\nrounds: 5"
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
          "id": "4c7db4b4158e50e7c4116d8eeec8da8e3e89ea29",
          "message": "Update version",
          "timestamp": "2025-10-05T12:20:22+02:00",
          "tree_id": "f685e8c429205551dd3e17836320ac222b2e6b11",
          "url": "https://github.com/uroni/hs5/commit/4c7db4b4158e50e7c4116d8eeec8da8e3e89ea29"
        },
        "date": 1759660670311,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06994679817055581,
            "unit": "iter/sec",
            "range": "stddev: 0.16644980316547134",
            "extra": "mean: 14.296580060199972 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06305187107840468,
            "unit": "iter/sec",
            "range": "stddev: 0.22402725146125302",
            "extra": "mean: 15.85995756980003 sec\nrounds: 5"
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
          "id": "8eeef7df1177c29885ef1f03730ceac2ee9a6a47",
          "message": "Don't use epoll_pwait2\n\nDoesn't work on Linux < 5.11",
          "timestamp": "2025-10-11T19:45:00+02:00",
          "tree_id": "a83b340ba9337a21c17a8c561b6250facb27fae2",
          "url": "https://github.com/uroni/hs5/commit/8eeef7df1177c29885ef1f03730ceac2ee9a6a47"
        },
        "date": 1760205243101,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.075890860235325,
            "unit": "iter/sec",
            "range": "stddev: 0.1603989312377246",
            "extra": "mean: 13.176817299200001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06518064869183654,
            "unit": "iter/sec",
            "range": "stddev: 0.06463537464344574",
            "extra": "mean: 15.341976799400026 sec\nrounds: 5"
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
          "id": "a9b445710129673056807db35ec7a8ccf33e56a3",
          "message": "Increment version",
          "timestamp": "2025-10-11T20:29:55+02:00",
          "tree_id": "ac1b1fd682d2e61d2d2799ba7cf76952a09af34c",
          "url": "https://github.com/uroni/hs5/commit/a9b445710129673056807db35ec7a8ccf33e56a3"
        },
        "date": 1760211490631,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0716136572592727,
            "unit": "iter/sec",
            "range": "stddev: 0.16109293079067197",
            "extra": "mean: 13.96381693480007 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06648068357448915,
            "unit": "iter/sec",
            "range": "stddev: 0.24228274271973918",
            "extra": "mean: 15.041963262599984 sec\nrounds: 5"
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
          "id": "a9b445710129673056807db35ec7a8ccf33e56a3",
          "message": "Increment version",
          "timestamp": "2025-10-11T20:29:55+02:00",
          "tree_id": "ac1b1fd682d2e61d2d2799ba7cf76952a09af34c",
          "url": "https://github.com/uroni/hs5/commit/a9b445710129673056807db35ec7a8ccf33e56a3"
        },
        "date": 1760211519620,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07583151486251541,
            "unit": "iter/sec",
            "range": "stddev: 0.11229062250209801",
            "extra": "mean: 13.187129412000104 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06853871572920779,
            "unit": "iter/sec",
            "range": "stddev: 0.20832105727685254",
            "extra": "mean: 14.59029381220007 sec\nrounds: 5"
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
          "id": "b85fd17525d97833a21f4463090487d3a7c407d2",
          "message": "Improve write error handling",
          "timestamp": "2025-10-12T12:27:26+02:00",
          "tree_id": "da02d1890673f3335ce81e96940f7fe3dff300f1",
          "url": "https://github.com/uroni/hs5/commit/b85fd17525d97833a21f4463090487d3a7c407d2"
        },
        "date": 1760265379172,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07629484485382028,
            "unit": "iter/sec",
            "range": "stddev: 0.049977700901608",
            "extra": "mean: 13.107045461799999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06559028385520889,
            "unit": "iter/sec",
            "range": "stddev: 0.1361748339696308",
            "extra": "mean: 15.246160577800037 sec\nrounds: 5"
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
          "id": "9e5231226a6ee39e79b3eaeae4a6331423f813fb",
          "message": "Fix arm release build",
          "timestamp": "2025-10-12T12:39:23+02:00",
          "tree_id": "c0ffb0792d017e1cd637a41b5c1473ace8e628b5",
          "url": "https://github.com/uroni/hs5/commit/9e5231226a6ee39e79b3eaeae4a6331423f813fb"
        },
        "date": 1760266106256,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06790668500726343,
            "unit": "iter/sec",
            "range": "stddev: 0.2593327021049957",
            "extra": "mean: 14.726090662399997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06147510270922382,
            "unit": "iter/sec",
            "range": "stddev: 0.2385834896975967",
            "extra": "mean: 16.26674793420001 sec\nrounds: 5"
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
          "id": "a19b81d5ebcf646ebbf9bea0fd01b42afb526ecb",
          "message": "Remove weird import",
          "timestamp": "2025-10-12T14:45:52+02:00",
          "tree_id": "5bfa59a88fdd8043f60f2f10b409339e60b1bf8a",
          "url": "https://github.com/uroni/hs5/commit/a19b81d5ebcf646ebbf9bea0fd01b42afb526ecb"
        },
        "date": 1760273703094,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07441800487333392,
            "unit": "iter/sec",
            "range": "stddev: 0.19364634329580122",
            "extra": "mean: 13.437608300599958 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06550222398288197,
            "unit": "iter/sec",
            "range": "stddev: 0.3368577073194423",
            "extra": "mean: 15.266657209400023 sec\nrounds: 5"
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
          "id": "a19b81d5ebcf646ebbf9bea0fd01b42afb526ecb",
          "message": "Remove weird import",
          "timestamp": "2025-10-12T14:45:52+02:00",
          "tree_id": "5bfa59a88fdd8043f60f2f10b409339e60b1bf8a",
          "url": "https://github.com/uroni/hs5/commit/a19b81d5ebcf646ebbf9bea0fd01b42afb526ecb"
        },
        "date": 1760274063716,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07601461235789406,
            "unit": "iter/sec",
            "range": "stddev: 0.12552069796726792",
            "extra": "mean: 13.155365382799994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.057083581916729714,
            "unit": "iter/sec",
            "range": "stddev: 1.2215150246581952",
            "extra": "mean: 17.518171888000005 sec\nrounds: 5"
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
          "id": "dc25372f9e693c3948798f37bf19b8fbc5ff61d3",
          "message": "Increment version",
          "timestamp": "2025-10-23T23:44:58+02:00",
          "tree_id": "a90b2e94c9332571276d1b58bd82505717119b73",
          "url": "https://github.com/uroni/hs5/commit/dc25372f9e693c3948798f37bf19b8fbc5ff61d3"
        },
        "date": 1761256440955,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07635481811587207,
            "unit": "iter/sec",
            "range": "stddev: 0.16678124181308931",
            "extra": "mean: 13.096750469400012 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06584202633008096,
            "unit": "iter/sec",
            "range": "stddev: 0.20959241402019557",
            "extra": "mean: 15.187867927800005 sec\nrounds: 5"
          }
        ]
      }
    ]
  }
}