window.BENCHMARK_DATA = {
  "lastUpdate": 1752335554246,
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
      }
    ]
  }
}