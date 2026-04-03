window.BENCHMARK_DATA = {
  "lastUpdate": 1775227165982,
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
        "date": 1761256452678,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07620762919990935,
            "unit": "iter/sec",
            "range": "stddev: 0.13625382040298029",
            "extra": "mean: 13.12204579119999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06699162308281573,
            "unit": "iter/sec",
            "range": "stddev: 0.1763807618516994",
            "extra": "mean: 14.9272394664 sec\nrounds: 5"
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
          "id": "49a9b36839d9be14e7ee9a1b70259252e7fbc098",
          "message": "Increment version",
          "timestamp": "2025-10-24T00:15:08+02:00",
          "tree_id": "9b9a3cf14b75a3084b50abe917d6ee71e12ff9a2",
          "url": "https://github.com/uroni/hs5/commit/49a9b36839d9be14e7ee9a1b70259252e7fbc098"
        },
        "date": 1761258223849,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07850215473536239,
            "unit": "iter/sec",
            "range": "stddev: 0.16697171026957106",
            "extra": "mean: 12.738503845799993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06711788206867081,
            "unit": "iter/sec",
            "range": "stddev: 0.19802760508288084",
            "extra": "mean: 14.899159049400021 sec\nrounds: 5"
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
          "id": "49a9b36839d9be14e7ee9a1b70259252e7fbc098",
          "message": "Increment version",
          "timestamp": "2025-10-24T00:15:08+02:00",
          "tree_id": "9b9a3cf14b75a3084b50abe917d6ee71e12ff9a2",
          "url": "https://github.com/uroni/hs5/commit/49a9b36839d9be14e7ee9a1b70259252e7fbc098"
        },
        "date": 1761258286195,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07585883058447582,
            "unit": "iter/sec",
            "range": "stddev: 0.2380013319869431",
            "extra": "mean: 13.182380907999994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06579739524819751,
            "unit": "iter/sec",
            "range": "stddev: 0.2187528393890321",
            "extra": "mean: 15.198170022199998 sec\nrounds: 5"
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
          "id": "5925327e877097fcca482d1c6306191db1a1cea8",
          "message": "Add section about WAL",
          "timestamp": "2025-11-09T14:24:56+01:00",
          "tree_id": "becaf662a18f2777d8890632e7d00ad412bf2b4d",
          "url": "https://github.com/uroni/hs5/commit/5925327e877097fcca482d1c6306191db1a1cea8"
        },
        "date": 1762804140078,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07171314126702802,
            "unit": "iter/sec",
            "range": "stddev: 0.6318161094721969",
            "extra": "mean: 13.944445639000003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06452801190728179,
            "unit": "iter/sec",
            "range": "stddev: 0.28347794703205825",
            "extra": "mean: 15.497145664999994 sec\nrounds: 5"
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
          "id": "5925327e877097fcca482d1c6306191db1a1cea8",
          "message": "Add section about WAL",
          "timestamp": "2025-11-09T14:24:56+01:00",
          "tree_id": "becaf662a18f2777d8890632e7d00ad412bf2b4d",
          "url": "https://github.com/uroni/hs5/commit/5925327e877097fcca482d1c6306191db1a1cea8"
        },
        "date": 1762807077499,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07850225967452124,
            "unit": "iter/sec",
            "range": "stddev: 0.14608976968968945",
            "extra": "mean: 12.738486817399997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06959080571901115,
            "unit": "iter/sec",
            "range": "stddev: 0.14957276022653618",
            "extra": "mean: 14.369714356200006 sec\nrounds: 5"
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
          "id": "df2167e9d442052bcc7f6741417db44561c4c972",
          "message": "Test doing many simultaneous range downloads",
          "timestamp": "2025-11-11T20:00:12+01:00",
          "tree_id": "dadcf8a2fff1d17f656671fb55d8342e1f93342b",
          "url": "https://github.com/uroni/hs5/commit/df2167e9d442052bcc7f6741417db44561c4c972"
        },
        "date": 1762888113131,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07290417712340726,
            "unit": "iter/sec",
            "range": "stddev: 0.2805919456818799",
            "extra": "mean: 13.716635170400014 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06606066609232752,
            "unit": "iter/sec",
            "range": "stddev: 0.2871446305493628",
            "extra": "mean: 15.137600922800004 sec\nrounds: 5"
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
          "id": "be30fa1d206e6af4c1ebd6bb17893a0c8cb742cc",
          "message": "Increment version",
          "timestamp": "2025-11-11T20:01:07+01:00",
          "tree_id": "ef3f219c5a7646302c6cb7a20436854129264902",
          "url": "https://github.com/uroni/hs5/commit/be30fa1d206e6af4c1ebd6bb17893a0c8cb742cc"
        },
        "date": 1762888234747,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06736410543404282,
            "unit": "iter/sec",
            "range": "stddev: 0.04865914497341481",
            "extra": "mean: 14.844700951000004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0611859086094261,
            "unit": "iter/sec",
            "range": "stddev: 0.11159220342520741",
            "extra": "mean: 16.343632426599992 sec\nrounds: 5"
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
          "id": "be30fa1d206e6af4c1ebd6bb17893a0c8cb742cc",
          "message": "Increment version",
          "timestamp": "2025-11-11T20:01:07+01:00",
          "tree_id": "ef3f219c5a7646302c6cb7a20436854129264902",
          "url": "https://github.com/uroni/hs5/commit/be30fa1d206e6af4c1ebd6bb17893a0c8cb742cc"
        },
        "date": 1762892434459,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09041384201579929,
            "unit": "iter/sec",
            "range": "stddev: 0.10814146042020302",
            "extra": "mean: 11.060253360599983 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07988895203031153,
            "unit": "iter/sec",
            "range": "stddev: 0.15635048540220953",
            "extra": "mean: 12.517375364000008 sec\nrounds: 5"
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
          "id": "312f12ccfe5cea75d04bcba83372177533d33764",
          "message": "Fix build issue",
          "timestamp": "2025-11-11T23:09:43+01:00",
          "tree_id": "d3927b72fe8191c426d9983e6937fbf9489c812c",
          "url": "https://github.com/uroni/hs5/commit/312f12ccfe5cea75d04bcba83372177533d33764"
        },
        "date": 1762899462790,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07828333990509853,
            "unit": "iter/sec",
            "range": "stddev: 0.18221965044634583",
            "extra": "mean: 12.774110062400018 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06880564678675885,
            "unit": "iter/sec",
            "range": "stddev: 0.20808006528292441",
            "extra": "mean: 14.533690862599997 sec\nrounds: 5"
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
          "id": "312f12ccfe5cea75d04bcba83372177533d33764",
          "message": "Fix build issue",
          "timestamp": "2025-11-11T23:09:43+01:00",
          "tree_id": "d3927b72fe8191c426d9983e6937fbf9489c812c",
          "url": "https://github.com/uroni/hs5/commit/312f12ccfe5cea75d04bcba83372177533d33764"
        },
        "date": 1762899560072,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07235333586744196,
            "unit": "iter/sec",
            "range": "stddev: 0.1814103283990351",
            "extra": "mean: 13.821062816399968 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06545433839671967,
            "unit": "iter/sec",
            "range": "stddev: 0.23105411845947296",
            "extra": "mean: 15.277826107400028 sec\nrounds: 5"
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
          "id": "977af7d011fa998ac7c54796a367208f9fa5849b",
          "message": "Try using default linker",
          "timestamp": "2025-11-11T23:40:57+01:00",
          "tree_id": "c247d5121bb52632667ada09aa6a2183d6e8c284",
          "url": "https://github.com/uroni/hs5/commit/977af7d011fa998ac7c54796a367208f9fa5849b"
        },
        "date": 1762901391120,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07169212408353662,
            "unit": "iter/sec",
            "range": "stddev: 0.3019571925481245",
            "extra": "mean: 13.94853357720001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06487203892015916,
            "unit": "iter/sec",
            "range": "stddev: 0.2728818226193208",
            "extra": "mean: 15.414961771599986 sec\nrounds: 5"
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
          "id": "977af7d011fa998ac7c54796a367208f9fa5849b",
          "message": "Try using default linker",
          "timestamp": "2025-11-11T23:40:57+01:00",
          "tree_id": "c247d5121bb52632667ada09aa6a2183d6e8c284",
          "url": "https://github.com/uroni/hs5/commit/977af7d011fa998ac7c54796a367208f9fa5849b"
        },
        "date": 1762901397918,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06987584865218112,
            "unit": "iter/sec",
            "range": "stddev: 0.13613281151568687",
            "extra": "mean: 14.311096312799998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06371644398204446,
            "unit": "iter/sec",
            "range": "stddev: 0.2355232111813353",
            "extra": "mean: 15.694535625399999 sec\nrounds: 5"
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
          "id": "977af7d011fa998ac7c54796a367208f9fa5849b",
          "message": "Try using default linker",
          "timestamp": "2025-11-11T23:40:57+01:00",
          "tree_id": "c247d5121bb52632667ada09aa6a2183d6e8c284",
          "url": "https://github.com/uroni/hs5/commit/977af7d011fa998ac7c54796a367208f9fa5849b"
        },
        "date": 1762902073354,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07966318109031002,
            "unit": "iter/sec",
            "range": "stddev: 0.38451246424270064",
            "extra": "mean: 12.552850467599978 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06881152356263985,
            "unit": "iter/sec",
            "range": "stddev: 0.38236734143133444",
            "extra": "mean: 14.532449628000018 sec\nrounds: 5"
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
          "id": "78ced7f66635ffa984567cb3257c2514be057244",
          "message": "Increment version",
          "timestamp": "2025-11-11T23:51:59+01:00",
          "tree_id": "525ff74cb42edef7df3deb0073ea33d503cdd593",
          "url": "https://github.com/uroni/hs5/commit/78ced7f66635ffa984567cb3257c2514be057244"
        },
        "date": 1762902169217,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07531957886606361,
            "unit": "iter/sec",
            "range": "stddev: 0.17272822499000298",
            "extra": "mean: 13.276760372999979 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06819528550180112,
            "unit": "iter/sec",
            "range": "stddev: 0.19053422906155798",
            "extra": "mean: 14.6637702686 sec\nrounds: 5"
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
          "id": "78ced7f66635ffa984567cb3257c2514be057244",
          "message": "Increment version",
          "timestamp": "2025-11-11T23:51:59+01:00",
          "tree_id": "525ff74cb42edef7df3deb0073ea33d503cdd593",
          "url": "https://github.com/uroni/hs5/commit/78ced7f66635ffa984567cb3257c2514be057244"
        },
        "date": 1762902187952,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07610499973298676,
            "unit": "iter/sec",
            "range": "stddev: 0.1294908352818151",
            "extra": "mean: 13.139741193199985 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06728182003674069,
            "unit": "iter/sec",
            "range": "stddev: 0.19514138477113568",
            "extra": "mean: 14.86285596099999 sec\nrounds: 5"
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
          "id": "3669468130fad1b8efaca161bc97145694456a2a",
          "message": "Fix setting up data-only WAL mode",
          "timestamp": "2025-11-12T00:11:59+01:00",
          "tree_id": "6ea37c84dd809bcef765788de9cbc2832299f253",
          "url": "https://github.com/uroni/hs5/commit/3669468130fad1b8efaca161bc97145694456a2a"
        },
        "date": 1762903245079,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07369174091451013,
            "unit": "iter/sec",
            "range": "stddev: 0.09761886522429555",
            "extra": "mean: 13.570041738600002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06573569109872401,
            "unit": "iter/sec",
            "range": "stddev: 0.2612045871924903",
            "extra": "mean: 15.21243609500001 sec\nrounds: 5"
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
          "id": "3669468130fad1b8efaca161bc97145694456a2a",
          "message": "Fix setting up data-only WAL mode",
          "timestamp": "2025-11-12T00:11:59+01:00",
          "tree_id": "6ea37c84dd809bcef765788de9cbc2832299f253",
          "url": "https://github.com/uroni/hs5/commit/3669468130fad1b8efaca161bc97145694456a2a"
        },
        "date": 1762903266491,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07260629215976475,
            "unit": "iter/sec",
            "range": "stddev: 0.1440222847159129",
            "extra": "mean: 13.772911000599981 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0655411273503943,
            "unit": "iter/sec",
            "range": "stddev: 0.2134706559179316",
            "extra": "mean: 15.257595351600003 sec\nrounds: 5"
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
          "id": "9ace96b97d6e1876e398c8854664ae584153f66d",
          "message": "Increment version",
          "timestamp": "2025-11-13T20:17:11+01:00",
          "tree_id": "4831cf665f727cdbf31aa85fe4fded978ebf8819",
          "url": "https://github.com/uroni/hs5/commit/9ace96b97d6e1876e398c8854664ae584153f66d"
        },
        "date": 1763062664224,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07650284139653048,
            "unit": "iter/sec",
            "range": "stddev: 0.07266611518842248",
            "extra": "mean: 13.071409920800033 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06663472903018942,
            "unit": "iter/sec",
            "range": "stddev: 0.31582048690432224",
            "extra": "mean: 15.007189412399976 sec\nrounds: 5"
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
          "id": "9ace96b97d6e1876e398c8854664ae584153f66d",
          "message": "Increment version",
          "timestamp": "2025-11-13T20:17:11+01:00",
          "tree_id": "4831cf665f727cdbf31aa85fe4fded978ebf8819",
          "url": "https://github.com/uroni/hs5/commit/9ace96b97d6e1876e398c8854664ae584153f66d"
        },
        "date": 1763062783003,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07242115553111814,
            "unit": "iter/sec",
            "range": "stddev: 0.1907824935916389",
            "extra": "mean: 13.808119915600036 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06431267822976708,
            "unit": "iter/sec",
            "range": "stddev: 0.25808651617347",
            "extra": "mean: 15.549033682399976 sec\nrounds: 5"
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
          "id": "5893084caccb2d4a97f1d6c74e5022908ed4bd14",
          "message": "Set thread names",
          "timestamp": "2025-11-15T12:17:45+01:00",
          "tree_id": "61bb622f94ec64b7bd4e234a3ebdc16cf21fe1be",
          "url": "https://github.com/uroni/hs5/commit/5893084caccb2d4a97f1d6c74e5022908ed4bd14"
        },
        "date": 1763205954238,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07922001833456128,
            "unit": "iter/sec",
            "range": "stddev: 0.08223393915044828",
            "extra": "mean: 12.623072059599997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07295380615581827,
            "unit": "iter/sec",
            "range": "stddev: 0.1828972859727485",
            "extra": "mean: 13.707304014600027 sec\nrounds: 5"
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
          "id": "f1a7b3430f574dfdb99cb2a0c9e2c7c5329498f1",
          "message": "Revert debugging change",
          "timestamp": "2025-11-26T22:23:10+01:00",
          "tree_id": "f74661ab71d09ad2eb68f018513c68803dcade1e",
          "url": "https://github.com/uroni/hs5/commit/f1a7b3430f574dfdb99cb2a0c9e2c7c5329498f1"
        },
        "date": 1764192798076,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07721674497586987,
            "unit": "iter/sec",
            "range": "stddev: 0.1306374125157109",
            "extra": "mean: 12.950558849799984 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0671784796054103,
            "unit": "iter/sec",
            "range": "stddev: 0.12656850647013293",
            "extra": "mean: 14.885719442800006 sec\nrounds: 5"
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
          "id": "f1a7b3430f574dfdb99cb2a0c9e2c7c5329498f1",
          "message": "Revert debugging change",
          "timestamp": "2025-11-26T22:23:10+01:00",
          "tree_id": "f74661ab71d09ad2eb68f018513c68803dcade1e",
          "url": "https://github.com/uroni/hs5/commit/f1a7b3430f574dfdb99cb2a0c9e2c7c5329498f1"
        },
        "date": 1764193826583,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07831024020331116,
            "unit": "iter/sec",
            "range": "stddev: 0.08407240079250283",
            "extra": "mean: 12.769722036400003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06934630299544772,
            "unit": "iter/sec",
            "range": "stddev: 0.15144931387424487",
            "extra": "mean: 14.420379411800013 sec\nrounds: 5"
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
          "id": "e51928d4753751ea9ddc3b1f314f2744c1ae72cd",
          "message": "Add debug launch config",
          "timestamp": "2025-11-30T20:46:43+01:00",
          "tree_id": "25c91c718d23bca3858ae3c3ade3aa7d47154366",
          "url": "https://github.com/uroni/hs5/commit/e51928d4753751ea9ddc3b1f314f2744c1ae72cd"
        },
        "date": 1764532470719,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07701754721642266,
            "unit": "iter/sec",
            "range": "stddev: 0.05193098102760953",
            "extra": "mean: 12.984054103799963 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06735311280347357,
            "unit": "iter/sec",
            "range": "stddev: 0.11276442194018156",
            "extra": "mean: 14.84712373900004 sec\nrounds: 5"
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
          "id": "e51928d4753751ea9ddc3b1f314f2744c1ae72cd",
          "message": "Add debug launch config",
          "timestamp": "2025-11-30T20:46:43+01:00",
          "tree_id": "25c91c718d23bca3858ae3c3ade3aa7d47154366",
          "url": "https://github.com/uroni/hs5/commit/e51928d4753751ea9ddc3b1f314f2744c1ae72cd"
        },
        "date": 1764533236586,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07161144928072741,
            "unit": "iter/sec",
            "range": "stddev: 0.3739795295710987",
            "extra": "mean: 13.964247477799995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06314621721802934,
            "unit": "iter/sec",
            "range": "stddev: 0.24455536386420598",
            "extra": "mean: 15.836261363799997 sec\nrounds: 5"
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
          "id": "395e2cc0bf0749b9deb09d750c7ecf06abbd844c",
          "message": "Decrement pending items when it is not the last pending item",
          "timestamp": "2025-12-01T20:37:36+01:00",
          "tree_id": "ca1a3bdaba0e253f459532446b8a95079b51bf49",
          "url": "https://github.com/uroni/hs5/commit/395e2cc0bf0749b9deb09d750c7ecf06abbd844c"
        },
        "date": 1764620285017,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07459105885172096,
            "unit": "iter/sec",
            "range": "stddev: 0.09763336099185138",
            "extra": "mean: 13.406432559000041 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06611646284620107,
            "unit": "iter/sec",
            "range": "stddev: 0.1701177513906117",
            "extra": "mean: 15.124826056199982 sec\nrounds: 5"
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
          "id": "395e2cc0bf0749b9deb09d750c7ecf06abbd844c",
          "message": "Decrement pending items when it is not the last pending item",
          "timestamp": "2025-12-01T20:37:36+01:00",
          "tree_id": "ca1a3bdaba0e253f459532446b8a95079b51bf49",
          "url": "https://github.com/uroni/hs5/commit/395e2cc0bf0749b9deb09d750c7ecf06abbd844c"
        },
        "date": 1764620326794,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07249114325524461,
            "unit": "iter/sec",
            "range": "stddev: 0.15450318740714275",
            "extra": "mean: 13.79478864720004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0637539790679624,
            "unit": "iter/sec",
            "range": "stddev: 0.12444284324572491",
            "extra": "mean: 15.6852954846001 sec\nrounds: 5"
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
          "id": "a553f24c471cd2ce29ced3d4f3bb52b6a51dd648",
          "message": "Write wal file header in correct circumstances",
          "timestamp": "2025-12-01T21:54:55+01:00",
          "tree_id": "71af3ffc6adf93bf53c31638d335c18fd96a989d",
          "url": "https://github.com/uroni/hs5/commit/a553f24c471cd2ce29ced3d4f3bb52b6a51dd648"
        },
        "date": 1764622994280,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07371894944550458,
            "unit": "iter/sec",
            "range": "stddev: 0.1488012352457825",
            "extra": "mean: 13.565033244799997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06622881752780835,
            "unit": "iter/sec",
            "range": "stddev: 0.09499801215705589",
            "extra": "mean: 15.099167361399997 sec\nrounds: 5"
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
          "id": "a553f24c471cd2ce29ced3d4f3bb52b6a51dd648",
          "message": "Write wal file header in correct circumstances",
          "timestamp": "2025-12-01T21:54:55+01:00",
          "tree_id": "71af3ffc6adf93bf53c31638d335c18fd96a989d",
          "url": "https://github.com/uroni/hs5/commit/a553f24c471cd2ce29ced3d4f3bb52b6a51dd648"
        },
        "date": 1764623006235,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07432101509154039,
            "unit": "iter/sec",
            "range": "stddev: 0.08368942885767901",
            "extra": "mean: 13.455144534400006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06676465232826134,
            "unit": "iter/sec",
            "range": "stddev: 0.23759939250294224",
            "extra": "mean: 14.977985582600002 sec\nrounds: 5"
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
          "id": "19fb154fbee1baa8eb7a22d5eaa31cf7cd055fab",
          "message": "URL decode object paths if necessary",
          "timestamp": "2025-12-04T22:29:11+01:00",
          "tree_id": "b37f70c21cdb4ddfa592189669ac12f5355828b3",
          "url": "https://github.com/uroni/hs5/commit/19fb154fbee1baa8eb7a22d5eaa31cf7cd055fab"
        },
        "date": 1764884280228,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07765626953572086,
            "unit": "iter/sec",
            "range": "stddev: 0.12919731592707218",
            "extra": "mean: 12.877260341999989 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06902390165030896,
            "unit": "iter/sec",
            "range": "stddev: 0.11016571001394478",
            "extra": "mean: 14.487735061199976 sec\nrounds: 5"
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
          "id": "19fb154fbee1baa8eb7a22d5eaa31cf7cd055fab",
          "message": "URL decode object paths if necessary",
          "timestamp": "2025-12-04T22:29:11+01:00",
          "tree_id": "b37f70c21cdb4ddfa592189669ac12f5355828b3",
          "url": "https://github.com/uroni/hs5/commit/19fb154fbee1baa8eb7a22d5eaa31cf7cd055fab"
        },
        "date": 1764884405817,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07380014576747365,
            "unit": "iter/sec",
            "range": "stddev: 0.08626092087526889",
            "extra": "mean: 13.550108737600022 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0656578525060397,
            "unit": "iter/sec",
            "range": "stddev: 0.28639555644506254",
            "extra": "mean: 15.230470717999992 sec\nrounds: 5"
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
          "id": "4457133406edf58879138e3eb3a2421d0bb46197",
          "message": "Remove obsolete abort code\n\nReplaced by keeping track\nof parts per upload in db",
          "timestamp": "2026-01-22T14:02:13+01:00",
          "tree_id": "a27f89a449b2d766163bae4ddbf688054a1ef1b0",
          "url": "https://github.com/uroni/hs5/commit/4457133406edf58879138e3eb3a2421d0bb46197"
        },
        "date": 1769090192649,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.074685994068179,
            "unit": "iter/sec",
            "range": "stddev: 0.16887067711235926",
            "extra": "mean: 13.389391310600013 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06507771602472995,
            "unit": "iter/sec",
            "range": "stddev: 0.26132071588425526",
            "extra": "mean: 15.366243025800008 sec\nrounds: 5"
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
          "id": "4457133406edf58879138e3eb3a2421d0bb46197",
          "message": "Remove obsolete abort code\n\nReplaced by keeping track\nof parts per upload in db",
          "timestamp": "2026-01-22T14:02:13+01:00",
          "tree_id": "a27f89a449b2d766163bae4ddbf688054a1ef1b0",
          "url": "https://github.com/uroni/hs5/commit/4457133406edf58879138e3eb3a2421d0bb46197"
        },
        "date": 1769091572614,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.075838312341263,
            "unit": "iter/sec",
            "range": "stddev: 0.13023903984804713",
            "extra": "mean: 13.185947433800004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06708154137488373,
            "unit": "iter/sec",
            "range": "stddev: 0.16839264335748913",
            "extra": "mean: 14.907230506399992 sec\nrounds: 5"
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
          "id": "e6713a8c941a705ae244e672cadca2534504091b",
          "message": "Add feature to create initial buckets via params",
          "timestamp": "2026-02-13T10:53:08+01:00",
          "tree_id": "276fd859af986b1d3405aecf97aa017767f8d329",
          "url": "https://github.com/uroni/hs5/commit/e6713a8c941a705ae244e672cadca2534504091b"
        },
        "date": 1770977067281,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06674100840273099,
            "unit": "iter/sec",
            "range": "stddev: 0.17322586664615772",
            "extra": "mean: 14.983291741200015 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.060110334260532375,
            "unit": "iter/sec",
            "range": "stddev: 0.10705622033253585",
            "extra": "mean: 16.63607451700009 sec\nrounds: 5"
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
          "id": "e6713a8c941a705ae244e672cadca2534504091b",
          "message": "Add feature to create initial buckets via params",
          "timestamp": "2026-02-13T10:53:08+01:00",
          "tree_id": "276fd859af986b1d3405aecf97aa017767f8d329",
          "url": "https://github.com/uroni/hs5/commit/e6713a8c941a705ae244e672cadca2534504091b"
        },
        "date": 1770977595822,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07301817741480032,
            "unit": "iter/sec",
            "range": "stddev: 0.058209303539430636",
            "extra": "mean: 13.695219949399974 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06429441493898894,
            "unit": "iter/sec",
            "range": "stddev: 0.036886870974626625",
            "extra": "mean: 15.553450497199993 sec\nrounds: 5"
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
          "id": "9733ff305c011d9db89a1c37876caf234c120753",
          "message": "Fix title",
          "timestamp": "2026-02-14T21:28:40+01:00",
          "tree_id": "f9395f22f98203caeb70a2d6ee6739357b313d9c",
          "url": "https://github.com/uroni/hs5/commit/9733ff305c011d9db89a1c37876caf234c120753"
        },
        "date": 1771101491902,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07938095196694987,
            "unit": "iter/sec",
            "range": "stddev: 0.0848421139336011",
            "extra": "mean: 12.597480569599975 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07215081183748812,
            "unit": "iter/sec",
            "range": "stddev: 0.13552186076149036",
            "extra": "mean: 13.859857907800006 sec\nrounds: 5"
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
          "id": "79f797d6a7b2b9d2e725bb9b1c7c0016f3846232",
          "message": "Add tasks for homepage",
          "timestamp": "2026-02-15T13:59:47+01:00",
          "tree_id": "30a049abb66d87c72c6e14ec587d630f0eeab04b",
          "url": "https://github.com/uroni/hs5/commit/79f797d6a7b2b9d2e725bb9b1c7c0016f3846232"
        },
        "date": 1771160943635,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08074803365353903,
            "unit": "iter/sec",
            "range": "stddev: 0.05426775055610147",
            "extra": "mean: 12.384202497000024 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06906688187112507,
            "unit": "iter/sec",
            "range": "stddev: 0.4505130861419684",
            "extra": "mean: 14.47871936460001 sec\nrounds: 5"
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
          "id": "6ae0729e3c5c8feb677bf7b7306e58d39f3ee0d0",
          "message": "Use working-directory instead of cwd",
          "timestamp": "2026-02-15T14:12:07+01:00",
          "tree_id": "922c9f959a5b324fd79895b8015a869de5281a07",
          "url": "https://github.com/uroni/hs5/commit/6ae0729e3c5c8feb677bf7b7306e58d39f3ee0d0"
        },
        "date": 1771161732637,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06310217211161594,
            "unit": "iter/sec",
            "range": "stddev: 0.16369391400423647",
            "extra": "mean: 15.847315021600002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05727647627114675,
            "unit": "iter/sec",
            "range": "stddev: 0.24977862820776148",
            "extra": "mean: 17.459174605399983 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ad94f808ba52a01c9669a3e702a2848869fb706e",
          "message": "Enhance GitHub Actions workflow for Jekyll site\n\nUpdated workflow name for clarity and added Jekyll dependency installation step.",
          "timestamp": "2026-02-15T14:14:59+01:00",
          "tree_id": "20b4b3303c23a320df1e20b78d7262c29b7f4e02",
          "url": "https://github.com/uroni/hs5/commit/ad94f808ba52a01c9669a3e702a2848869fb706e"
        },
        "date": 1771161836586,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07147832490915193,
            "unit": "iter/sec",
            "range": "stddev: 0.1800340075009541",
            "extra": "mean: 13.990255105600022 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06364009354417773,
            "unit": "iter/sec",
            "range": "stddev: 0.07490604686050625",
            "extra": "mean: 15.713364709400048 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "76cb0624353f8ebf0a4b52c77c6e7012daf94c50",
          "message": "Change site URL in _config.yml\n\nUpdated the site URL in the configuration file.",
          "timestamp": "2026-02-15T14:17:00+01:00",
          "tree_id": "a1fe8527e680e31904f2f952dd4498d296e5a9e2",
          "url": "https://github.com/uroni/hs5/commit/76cb0624353f8ebf0a4b52c77c6e7012daf94c50"
        },
        "date": 1771161956267,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06944776530154388,
            "unit": "iter/sec",
            "range": "stddev: 0.3663090280099812",
            "extra": "mean: 14.399311419999993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05776303646587591,
            "unit": "iter/sec",
            "range": "stddev: 0.29262767665043155",
            "extra": "mean: 17.312109286200005 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5a0e1710a3e9852e5c64377ed939fe933ad92fb3",
          "message": "Disable site testing in pages-deploy.yml\n\nComment out the Test site step in the deployment workflow.",
          "timestamp": "2026-02-15T14:19:03+01:00",
          "tree_id": "ae7cfd2c8c12283559302df77c2ac3449738ddcd",
          "url": "https://github.com/uroni/hs5/commit/5a0e1710a3e9852e5c64377ed939fe933ad92fb3"
        },
        "date": 1771162084162,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07208349921801478,
            "unit": "iter/sec",
            "range": "stddev: 0.22902766847008782",
            "extra": "mean: 13.872800444600012 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06443241917818374,
            "unit": "iter/sec",
            "range": "stddev: 0.16548919595067993",
            "extra": "mean: 15.520137420799983 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f5c3c8bf87d575415bbdc62b0bc26ac7d4df8d08",
          "message": "Update artifact path for GitHub Pages deployment",
          "timestamp": "2026-02-15T14:22:16+01:00",
          "tree_id": "b05962132becd6980f4fbd05b57264b85a9e4b1d",
          "url": "https://github.com/uroni/hs5/commit/f5c3c8bf87d575415bbdc62b0bc26ac7d4df8d08"
        },
        "date": 1771162300368,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06982123228460416,
            "unit": "iter/sec",
            "range": "stddev: 0.061914495008349724",
            "extra": "mean: 14.322290903199995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06219008340217266,
            "unit": "iter/sec",
            "range": "stddev: 0.11913986413485801",
            "extra": "mean: 16.07973402340001 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3000ce09109b7095030cf70f2b96e66dccf01020",
          "message": "Modify Jekyll build command and artifact upload path\n\nUpdated Jekyll build command to use new baseurl and changed artifact path.",
          "timestamp": "2026-02-15T14:27:33+01:00",
          "tree_id": "84459e91b0486746bfa926ee05b93fe976d553ac",
          "url": "https://github.com/uroni/hs5/commit/3000ce09109b7095030cf70f2b96e66dccf01020"
        },
        "date": 1771162596873,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07104492571187006,
            "unit": "iter/sec",
            "range": "stddev: 0.38054190134320265",
            "extra": "mean: 14.075600614399992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06377811803438325,
            "unit": "iter/sec",
            "range": "stddev: 0.24308939921075481",
            "extra": "mean: 15.679358858800015 sec\nrounds: 5"
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
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "610c1cde3d621289bbfd868399c23cdd0477c7ec",
          "message": "Change site URL in configuration\n\nUpdate the site URL to include the '/hs5' path.",
          "timestamp": "2026-02-15T14:31:02+01:00",
          "tree_id": "7329419aced596199e63312b6bb213a9d3f1ad97",
          "url": "https://github.com/uroni/hs5/commit/610c1cde3d621289bbfd868399c23cdd0477c7ec"
        },
        "date": 1771162762727,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07027095145263963,
            "unit": "iter/sec",
            "range": "stddev: 0.8410365630197427",
            "extra": "mean: 14.230631282600006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06428914372579697,
            "unit": "iter/sec",
            "range": "stddev: 0.04265543136345702",
            "extra": "mean: 15.554725760000053 sec\nrounds: 5"
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
          "id": "c08fffab7c3a9175663293fbf409a74cc53feb4d",
          "message": "Delete zone identifier files",
          "timestamp": "2026-02-15T14:36:04+01:00",
          "tree_id": "f253e4e00cf46da5678689d372d97ce51bae9ce1",
          "url": "https://github.com/uroni/hs5/commit/c08fffab7c3a9175663293fbf409a74cc53feb4d"
        },
        "date": 1771163153546,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07206454122935003,
            "unit": "iter/sec",
            "range": "stddev: 0.10798723565107922",
            "extra": "mean: 13.876449956399995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06078598924367984,
            "unit": "iter/sec",
            "range": "stddev: 0.22861139007545173",
            "extra": "mean: 16.45115942740002 sec\nrounds: 5"
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
          "id": "5a26e1cd90528eeebcb0b2195e69d3ed324aed34",
          "message": "Change domain",
          "timestamp": "2026-02-15T15:05:47+01:00",
          "tree_id": "fb774e0511dce8a35d3d49537e7515b6a5edc20d",
          "url": "https://github.com/uroni/hs5/commit/5a26e1cd90528eeebcb0b2195e69d3ed324aed34"
        },
        "date": 1771164855697,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07445634213075893,
            "unit": "iter/sec",
            "range": "stddev: 0.04325010434755363",
            "extra": "mean: 13.430689332600002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0654762113259207,
            "unit": "iter/sec",
            "range": "stddev: 0.08204585068392606",
            "extra": "mean: 15.272722409400012 sec\nrounds: 5"
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
          "id": "5a26e1cd90528eeebcb0b2195e69d3ed324aed34",
          "message": "Change domain",
          "timestamp": "2026-02-15T15:05:47+01:00",
          "tree_id": "fb774e0511dce8a35d3d49537e7515b6a5edc20d",
          "url": "https://github.com/uroni/hs5/commit/5a26e1cd90528eeebcb0b2195e69d3ed324aed34"
        },
        "date": 1771165321320,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07179021848677043,
            "unit": "iter/sec",
            "range": "stddev: 0.03581187367368866",
            "extra": "mean: 13.92947425260004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0646576606484828,
            "unit": "iter/sec",
            "range": "stddev: 0.05515088966607256",
            "extra": "mean: 15.46607145959997 sec\nrounds: 5"
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
          "id": "8eaae190823638ef52bcd45150ac3c9e7d43ab84",
          "message": "Fix build issue",
          "timestamp": "2026-02-15T15:17:06+01:00",
          "tree_id": "776798b586761f07eba9604f32e5a3a45f495240",
          "url": "https://github.com/uroni/hs5/commit/8eaae190823638ef52bcd45150ac3c9e7d43ab84"
        },
        "date": 1771165603677,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07043053555368631,
            "unit": "iter/sec",
            "range": "stddev: 0.19114282370374946",
            "extra": "mean: 14.198386994199996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06243560670689455,
            "unit": "iter/sec",
            "range": "stddev: 0.29332917982839457",
            "extra": "mean: 16.01650168460001 sec\nrounds: 5"
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
          "id": "8eaae190823638ef52bcd45150ac3c9e7d43ab84",
          "message": "Fix build issue",
          "timestamp": "2026-02-15T15:17:06+01:00",
          "tree_id": "776798b586761f07eba9604f32e5a3a45f495240",
          "url": "https://github.com/uroni/hs5/commit/8eaae190823638ef52bcd45150ac3c9e7d43ab84"
        },
        "date": 1771165609101,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07360998197208074,
            "unit": "iter/sec",
            "range": "stddev: 0.02595421282046595",
            "extra": "mean: 13.585114045800015 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06619349141177941,
            "unit": "iter/sec",
            "range": "stddev: 0.09833529359572296",
            "extra": "mean: 15.107225478999975 sec\nrounds: 5"
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
          "id": "7502de1f78eb98ec37b9c546ea89875f690d9c51",
          "message": "Add user management api tests",
          "timestamp": "2026-02-15T20:34:04+01:00",
          "tree_id": "52d39cf78187dedd0817273b0066b1ca6623f682",
          "url": "https://github.com/uroni/hs5/commit/7502de1f78eb98ec37b9c546ea89875f690d9c51"
        },
        "date": 1771184559853,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08681567952532837,
            "unit": "iter/sec",
            "range": "stddev: 0.09635243357212268",
            "extra": "mean: 11.518656600600025 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0750080838034611,
            "unit": "iter/sec",
            "range": "stddev: 0.13604644562306906",
            "extra": "mean: 13.331896367600006 sec\nrounds: 5"
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
          "id": "7502de1f78eb98ec37b9c546ea89875f690d9c51",
          "message": "Add user management api tests",
          "timestamp": "2026-02-15T20:34:04+01:00",
          "tree_id": "52d39cf78187dedd0817273b0066b1ca6623f682",
          "url": "https://github.com/uroni/hs5/commit/7502de1f78eb98ec37b9c546ea89875f690d9c51"
        },
        "date": 1771185215414,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06808663808224036,
            "unit": "iter/sec",
            "range": "stddev: 0.04932876483877014",
            "extra": "mean: 14.68716958519999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06232259226890553,
            "unit": "iter/sec",
            "range": "stddev: 0.09968102468107914",
            "extra": "mean: 16.045545661599952 sec\nrounds: 5"
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
          "id": "a2f92402be8911587a14cefb05606b5e35ebc4ee",
          "message": "Explain default port",
          "timestamp": "2026-02-15T20:46:42+01:00",
          "tree_id": "4cd18ad7a87c4b2752992d6d993cb894fc6ce8c3",
          "url": "https://github.com/uroni/hs5/commit/a2f92402be8911587a14cefb05606b5e35ebc4ee"
        },
        "date": 1771185386679,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07057569004507916,
            "unit": "iter/sec",
            "range": "stddev: 0.10923819350048093",
            "extra": "mean: 14.169184876000008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06241720755016089,
            "unit": "iter/sec",
            "range": "stddev: 0.24985050947303367",
            "extra": "mean: 16.021222980799983 sec\nrounds: 5"
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
          "id": "a054a22426c30f4863736f116fbf2f99414fddb0",
          "message": "Fix init-root-access-key cmd param documentation",
          "timestamp": "2026-02-15T20:53:44+01:00",
          "tree_id": "50df356c271093fed66d2a9275221d901f95322c",
          "url": "https://github.com/uroni/hs5/commit/a054a22426c30f4863736f116fbf2f99414fddb0"
        },
        "date": 1771189860973,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0706919556818601,
            "unit": "iter/sec",
            "range": "stddev: 0.1421209183770296",
            "extra": "mean: 14.1458811028 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06058156968627712,
            "unit": "iter/sec",
            "range": "stddev: 0.4785112400456046",
            "extra": "mean: 16.506670348400018 sec\nrounds: 5"
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
          "id": "43e0323b194fd13d30bb682e8d7900bfed7cf251",
          "message": "Adjust for minio no longer being maintained",
          "timestamp": "2026-02-15T22:24:38+01:00",
          "tree_id": "5eed32a55a90087cddb885801b041b782a720205",
          "url": "https://github.com/uroni/hs5/commit/43e0323b194fd13d30bb682e8d7900bfed7cf251"
        },
        "date": 1771191278094,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07021122466053573,
            "unit": "iter/sec",
            "range": "stddev: 0.31801777713394525",
            "extra": "mean: 14.242736896200006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06387507653062954,
            "unit": "iter/sec",
            "range": "stddev: 0.2156357097473447",
            "extra": "mean: 15.655558541999984 sec\nrounds: 5"
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
          "id": "ee8fbf1f4056c6225c7bfd4b18906e576f912323",
          "message": "Skip manual commit if it is disabled",
          "timestamp": "2026-02-15T23:11:32+01:00",
          "tree_id": "f222c5741687412501cdd6d2f393dbf7a8007aa7",
          "url": "https://github.com/uroni/hs5/commit/ee8fbf1f4056c6225c7bfd4b18906e576f912323"
        },
        "date": 1771194035252,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07032570156772984,
            "unit": "iter/sec",
            "range": "stddev: 0.3786210109403663",
            "extra": "mean: 14.219552421199978 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06282304890273355,
            "unit": "iter/sec",
            "range": "stddev: 0.16212634840199147",
            "extra": "mean: 15.917724743800012 sec\nrounds: 5"
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
          "id": "e8d9fd86e9d9596754d9961d17a76312a65bc379",
          "message": "Link to python context manager",
          "timestamp": "2026-02-15T23:13:23+01:00",
          "tree_id": "a4257ae62a0a2fc57294d21ac975ed4db5be00ee",
          "url": "https://github.com/uroni/hs5/commit/e8d9fd86e9d9596754d9961d17a76312a65bc379"
        },
        "date": 1771194160648,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07409171342307352,
            "unit": "iter/sec",
            "range": "stddev: 0.021836412031224724",
            "extra": "mean: 13.496785993999993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06575192595864571,
            "unit": "iter/sec",
            "range": "stddev: 0.08971628943939955",
            "extra": "mean: 15.20867998040003 sec\nrounds: 5"
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
          "id": "8048c3e435b53a55e688767b60fbb82c2ba46452",
          "message": "Increment version",
          "timestamp": "2026-02-15T23:15:39+01:00",
          "tree_id": "36ca85d60116500a89d30f4f095e09e1b9907ffc",
          "url": "https://github.com/uroni/hs5/commit/8048c3e435b53a55e688767b60fbb82c2ba46452"
        },
        "date": 1771194250142,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07247995975134022,
            "unit": "iter/sec",
            "range": "stddev: 0.0911347293486721",
            "extra": "mean: 13.796917153800006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06452841801485584,
            "unit": "iter/sec",
            "range": "stddev: 0.05937893834437183",
            "extra": "mean: 15.497048134200009 sec\nrounds: 5"
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
          "id": "8048c3e435b53a55e688767b60fbb82c2ba46452",
          "message": "Increment version",
          "timestamp": "2026-02-15T23:15:39+01:00",
          "tree_id": "36ca85d60116500a89d30f4f095e09e1b9907ffc",
          "url": "https://github.com/uroni/hs5/commit/8048c3e435b53a55e688767b60fbb82c2ba46452"
        },
        "date": 1771194289787,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0703179373507667,
            "unit": "iter/sec",
            "range": "stddev: 0.05027655370212098",
            "extra": "mean: 14.221122485599995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06429779720363621,
            "unit": "iter/sec",
            "range": "stddev: 0.08771808696125138",
            "extra": "mean: 15.552632337199999 sec\nrounds: 5"
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
          "id": "4e689e5d5553fbf2e12cc141e26d8f19a9e3e462",
          "message": "Increment version",
          "timestamp": "2026-02-15T23:46:00+01:00",
          "tree_id": "3129bb49ed0053384ebc647b84339f1678b81134",
          "url": "https://github.com/uroni/hs5/commit/4e689e5d5553fbf2e12cc141e26d8f19a9e3e462"
        },
        "date": 1771196150083,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06519561788840923,
            "unit": "iter/sec",
            "range": "stddev: 0.1694325497194504",
            "extra": "mean: 15.338454215000002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06034796598686023,
            "unit": "iter/sec",
            "range": "stddev: 0.11892459320823887",
            "extra": "mean: 16.570566772999996 sec\nrounds: 5"
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
          "id": "2e5909a6644440bab11110b3990ec3c51b5e1777",
          "message": "Fix test",
          "timestamp": "2026-02-16T00:32:21+01:00",
          "tree_id": "8fd8ddc42b91ccb1b631b2f7055ea6a82b82ea8d",
          "url": "https://github.com/uroni/hs5/commit/2e5909a6644440bab11110b3990ec3c51b5e1777"
        },
        "date": 1771198863922,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08400390682755676,
            "unit": "iter/sec",
            "range": "stddev: 0.17020530403056874",
            "extra": "mean: 11.904208241799996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07305084221197498,
            "unit": "iter/sec",
            "range": "stddev: 0.12820974057835394",
            "extra": "mean: 13.689096110599985 sec\nrounds: 5"
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
          "id": "2e5909a6644440bab11110b3990ec3c51b5e1777",
          "message": "Fix test",
          "timestamp": "2026-02-16T00:32:21+01:00",
          "tree_id": "8fd8ddc42b91ccb1b631b2f7055ea6a82b82ea8d",
          "url": "https://github.com/uroni/hs5/commit/2e5909a6644440bab11110b3990ec3c51b5e1777"
        },
        "date": 1771199485675,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07397971146702371,
            "unit": "iter/sec",
            "range": "stddev: 0.03917153102738107",
            "extra": "mean: 13.517219520999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06579031180949982,
            "unit": "iter/sec",
            "range": "stddev: 0.06456201670843117",
            "extra": "mean: 15.199806362000015 sec\nrounds: 5"
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
          "id": "938629faa3c3b8fdd624d402de307c9013fdc1a8",
          "message": "Upgrade duckdb ui extension",
          "timestamp": "2026-02-16T10:25:47+01:00",
          "tree_id": "38a011d87deebe785508b2d9a50cbbba70dad5bf",
          "url": "https://github.com/uroni/hs5/commit/938629faa3c3b8fdd624d402de307c9013fdc1a8"
        },
        "date": 1771236284313,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07916598538887042,
            "unit": "iter/sec",
            "range": "stddev: 0.03937482269453104",
            "extra": "mean: 12.631687650799904 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0698725984923517,
            "unit": "iter/sec",
            "range": "stddev: 0.14669523709733953",
            "extra": "mean: 14.311762000800082 sec\nrounds: 5"
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
          "id": "938629faa3c3b8fdd624d402de307c9013fdc1a8",
          "message": "Upgrade duckdb ui extension",
          "timestamp": "2026-02-16T10:25:47+01:00",
          "tree_id": "38a011d87deebe785508b2d9a50cbbba70dad5bf",
          "url": "https://github.com/uroni/hs5/commit/938629faa3c3b8fdd624d402de307c9013fdc1a8"
        },
        "date": 1771236984609,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06966775871677308,
            "unit": "iter/sec",
            "range": "stddev: 0.22475536791401196",
            "extra": "mean: 14.353841984000008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06321908640231615,
            "unit": "iter/sec",
            "range": "stddev: 0.11301260702247005",
            "extra": "mean: 15.818007771199984 sec\nrounds: 5"
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
          "id": "98fb311f108defee4d3102fdc2583640b7fe4192",
          "message": "Increment version",
          "timestamp": "2026-02-16T11:57:38+01:00",
          "tree_id": "cf13e98406f6aaa1162761ac2c64a02b62a8d00f",
          "url": "https://github.com/uroni/hs5/commit/98fb311f108defee4d3102fdc2583640b7fe4192"
        },
        "date": 1771240084243,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06663408073099261,
            "unit": "iter/sec",
            "range": "stddev: 0.09788223996087002",
            "extra": "mean: 15.007335421000016 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05903590401576741,
            "unit": "iter/sec",
            "range": "stddev: 0.2882093752170469",
            "extra": "mean: 16.938844533199973 sec\nrounds: 5"
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
          "id": "98fb311f108defee4d3102fdc2583640b7fe4192",
          "message": "Increment version",
          "timestamp": "2026-02-16T11:57:38+01:00",
          "tree_id": "cf13e98406f6aaa1162761ac2c64a02b62a8d00f",
          "url": "https://github.com/uroni/hs5/commit/98fb311f108defee4d3102fdc2583640b7fe4192"
        },
        "date": 1771240107117,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06827988853513206,
            "unit": "iter/sec",
            "range": "stddev: 0.8829981634294791",
            "extra": "mean: 14.645600944199987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06488460037675486,
            "unit": "iter/sec",
            "range": "stddev: 0.06751942237105894",
            "extra": "mean: 15.41197748300001 sec\nrounds: 5"
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
          "id": "1a10a01d21cc846ce8a60bac8aea011020ab01e8",
          "message": "Add partNumber parameter support",
          "timestamp": "2026-02-18T11:57:54+01:00",
          "tree_id": "53e1230fc91d40c79836bf1c49aefeec6f330f9b",
          "url": "https://github.com/uroni/hs5/commit/1a10a01d21cc846ce8a60bac8aea011020ab01e8"
        },
        "date": 1771412952642,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06521450011163633,
            "unit": "iter/sec",
            "range": "stddev: 0.7137818924941287",
            "extra": "mean: 15.334013114999994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06242132894003831,
            "unit": "iter/sec",
            "range": "stddev: 0.40897595838052786",
            "extra": "mean: 16.020165174 sec\nrounds: 5"
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
          "id": "cc3e47a015b3a303a23e75664b29f30fef24e112",
          "message": "Increment version",
          "timestamp": "2026-02-18T12:08:41+01:00",
          "tree_id": "41fe35da9fc90f24a11445879c3026898967e83c",
          "url": "https://github.com/uroni/hs5/commit/cc3e47a015b3a303a23e75664b29f30fef24e112"
        },
        "date": 1771413523601,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07067471780343618,
            "unit": "iter/sec",
            "range": "stddev: 0.1633364072001098",
            "extra": "mean: 14.149331346200018 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06494139189149645,
            "unit": "iter/sec",
            "range": "stddev: 0.031100819836928178",
            "extra": "mean: 15.398499645199967 sec\nrounds: 5"
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
          "id": "cc3e47a015b3a303a23e75664b29f30fef24e112",
          "message": "Increment version",
          "timestamp": "2026-02-18T12:08:41+01:00",
          "tree_id": "41fe35da9fc90f24a11445879c3026898967e83c",
          "url": "https://github.com/uroni/hs5/commit/cc3e47a015b3a303a23e75664b29f30fef24e112"
        },
        "date": 1771413715973,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07275276485903356,
            "unit": "iter/sec",
            "range": "stddev: 0.19951258277895184",
            "extra": "mean: 13.745182082600014 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0639745271972007,
            "unit": "iter/sec",
            "range": "stddev: 0.20366513210106416",
            "extra": "mean: 15.631221422199996 sec\nrounds: 5"
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
          "id": "658a1b4f74fa3be0eb8358086fe71d23b6fdcadc",
          "message": "Use 403 forbidden instead of 401",
          "timestamp": "2026-02-18T16:11:19+01:00",
          "tree_id": "603b919726203ebe0ca4568f752fadd9987f4bd8",
          "url": "https://github.com/uroni/hs5/commit/658a1b4f74fa3be0eb8358086fe71d23b6fdcadc"
        },
        "date": 1771428045095,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08580451050200297,
            "unit": "iter/sec",
            "range": "stddev: 0.08174771832507016",
            "extra": "mean: 11.654398983800002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07484079298922758,
            "unit": "iter/sec",
            "range": "stddev: 0.13284183381174075",
            "extra": "mean: 13.361697011199999 sec\nrounds: 5"
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
          "id": "0972b9e2c371fb19629f72072f21fc7bc3e2827b",
          "message": "Fix delete bucket test",
          "timestamp": "2026-02-18T16:23:43+01:00",
          "tree_id": "89dfd327d629b242cc478bc13779297320ecaa46",
          "url": "https://github.com/uroni/hs5/commit/0972b9e2c371fb19629f72072f21fc7bc3e2827b"
        },
        "date": 1771428839684,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07229878191002985,
            "unit": "iter/sec",
            "range": "stddev: 0.36388382578489875",
            "extra": "mean: 13.831491673599999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05978562835882675,
            "unit": "iter/sec",
            "range": "stddev: 0.7802301722272029",
            "extra": "mean: 16.7264278632 sec\nrounds: 5"
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
          "id": "0972b9e2c371fb19629f72072f21fc7bc3e2827b",
          "message": "Fix delete bucket test",
          "timestamp": "2026-02-18T16:23:43+01:00",
          "tree_id": "89dfd327d629b242cc478bc13779297320ecaa46",
          "url": "https://github.com/uroni/hs5/commit/0972b9e2c371fb19629f72072f21fc7bc3e2827b"
        },
        "date": 1771432672057,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07095067435646475,
            "unit": "iter/sec",
            "range": "stddev: 0.07909071864606187",
            "extra": "mean: 14.094298737400004 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06426354143390192,
            "unit": "iter/sec",
            "range": "stddev: 0.04648233838216142",
            "extra": "mean: 15.560922689399979 sec\nrounds: 5"
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
          "id": "afcd47f55310cb228efe9531bcfbdc4352fbabc2",
          "message": "Improve bucket name parsing on creation",
          "timestamp": "2026-02-18T17:59:27+01:00",
          "tree_id": "d8679cb2b0aba280a731c11c472c1b797b3d8753",
          "url": "https://github.com/uroni/hs5/commit/afcd47f55310cb228efe9531bcfbdc4352fbabc2"
        },
        "date": 1771435102528,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07309514492860485,
            "unit": "iter/sec",
            "range": "stddev: 0.08499017709030443",
            "extra": "mean: 13.680799196400017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06576094877637448,
            "unit": "iter/sec",
            "range": "stddev: 0.14680308030670863",
            "extra": "mean: 15.206593253400012 sec\nrounds: 5"
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
          "id": "afcd47f55310cb228efe9531bcfbdc4352fbabc2",
          "message": "Improve bucket name parsing on creation",
          "timestamp": "2026-02-18T17:59:27+01:00",
          "tree_id": "d8679cb2b0aba280a731c11c472c1b797b3d8753",
          "url": "https://github.com/uroni/hs5/commit/afcd47f55310cb228efe9531bcfbdc4352fbabc2"
        },
        "date": 1771435197365,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07134086898277323,
            "unit": "iter/sec",
            "range": "stddev: 0.1381490795938607",
            "extra": "mean: 14.017210811400002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06309658324387499,
            "unit": "iter/sec",
            "range": "stddev: 0.2671828008700801",
            "extra": "mean: 15.848718719599981 sec\nrounds: 5"
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
          "id": "4520330a947076e538e1b44cd304cd56c13a5253",
          "message": "Make bucket deletion parsing more robust",
          "timestamp": "2026-02-18T18:24:50+01:00",
          "tree_id": "03d3673c7ad9898baadba957c2f841a708e08531",
          "url": "https://github.com/uroni/hs5/commit/4520330a947076e538e1b44cd304cd56c13a5253"
        },
        "date": 1771436102942,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07184634255697675,
            "unit": "iter/sec",
            "range": "stddev: 0.22057913210947733",
            "extra": "mean: 13.918592991800017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0644901297216267,
            "unit": "iter/sec",
            "range": "stddev: 0.12235556886702818",
            "extra": "mean: 15.506248852600015 sec\nrounds: 5"
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
          "id": "4520330a947076e538e1b44cd304cd56c13a5253",
          "message": "Make bucket deletion parsing more robust",
          "timestamp": "2026-02-18T18:24:50+01:00",
          "tree_id": "03d3673c7ad9898baadba957c2f841a708e08531",
          "url": "https://github.com/uroni/hs5/commit/4520330a947076e538e1b44cd304cd56c13a5253"
        },
        "date": 1771436104074,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06634631869317736,
            "unit": "iter/sec",
            "range": "stddev: 0.16484919532657621",
            "extra": "mean: 15.072426318399994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05834343215426818,
            "unit": "iter/sec",
            "range": "stddev: 0.1578115670037977",
            "extra": "mean: 17.13988983979997 sec\nrounds: 5"
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
          "id": "ef15e8999e955d6ae0286b5afabb4f2f370f435b",
          "message": "Parse bucket name correctly on deletion",
          "timestamp": "2026-02-18T19:09:38+01:00",
          "tree_id": "24e2a214186b5aa2bb21239f3b53c912383666d1",
          "url": "https://github.com/uroni/hs5/commit/ef15e8999e955d6ae0286b5afabb4f2f370f435b"
        },
        "date": 1771438771305,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07208690725417057,
            "unit": "iter/sec",
            "range": "stddev: 0.14495208778805632",
            "extra": "mean: 13.872144583399995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06447465944709904,
            "unit": "iter/sec",
            "range": "stddev: 0.11224943564785544",
            "extra": "mean: 15.50996947599998 sec\nrounds: 5"
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
          "id": "ef15e8999e955d6ae0286b5afabb4f2f370f435b",
          "message": "Parse bucket name correctly on deletion",
          "timestamp": "2026-02-18T19:09:38+01:00",
          "tree_id": "24e2a214186b5aa2bb21239f3b53c912383666d1",
          "url": "https://github.com/uroni/hs5/commit/ef15e8999e955d6ae0286b5afabb4f2f370f435b"
        },
        "date": 1771438804347,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06954331812544076,
            "unit": "iter/sec",
            "range": "stddev: 0.18986433385043627",
            "extra": "mean: 14.37952670300001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06303946278583174,
            "unit": "iter/sec",
            "range": "stddev: 0.297388185502188",
            "extra": "mean: 15.863079344400001 sec\nrounds: 5"
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
          "id": "c73613c4031472915f4b31c9cab3179d0f2fbb8e",
          "message": "Increment version",
          "timestamp": "2026-02-18T19:34:27+01:00",
          "tree_id": "ae527d94153acd47afbf28e957279c81a42ad405",
          "url": "https://github.com/uroni/hs5/commit/c73613c4031472915f4b31c9cab3179d0f2fbb8e"
        },
        "date": 1771440275937,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06894779873744421,
            "unit": "iter/sec",
            "range": "stddev: 0.2649749624635475",
            "extra": "mean: 14.503726272800055 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06324203269311243,
            "unit": "iter/sec",
            "range": "stddev: 0.14294794656274584",
            "extra": "mean: 15.812268477399972 sec\nrounds: 5"
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
          "id": "c73613c4031472915f4b31c9cab3179d0f2fbb8e",
          "message": "Increment version",
          "timestamp": "2026-02-18T19:34:27+01:00",
          "tree_id": "ae527d94153acd47afbf28e957279c81a42ad405",
          "url": "https://github.com/uroni/hs5/commit/c73613c4031472915f4b31c9cab3179d0f2fbb8e"
        },
        "date": 1771440302456,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07188479116084627,
            "unit": "iter/sec",
            "range": "stddev: 0.09149550447155946",
            "extra": "mean: 13.911148434199992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06446505155690634,
            "unit": "iter/sec",
            "range": "stddev: 0.12052380578690994",
            "extra": "mean: 15.512281086400014 sec\nrounds: 5"
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
          "id": "a27a922a79477a7a088242b9dcd347cb2fcffb1d",
          "message": "Add performance comparison with rustfs",
          "timestamp": "2026-02-18T20:42:30+01:00",
          "tree_id": "f6856cadcddb0d07f76ed1fed2ed0d33d86baa10",
          "url": "https://github.com/uroni/hs5/commit/a27a922a79477a7a088242b9dcd347cb2fcffb1d"
        },
        "date": 1771444323801,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07401894253085327,
            "unit": "iter/sec",
            "range": "stddev: 0.16167451381295556",
            "extra": "mean: 13.51005520760001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06621503760368695,
            "unit": "iter/sec",
            "range": "stddev: 0.08151625456846076",
            "extra": "mean: 15.102309629199976 sec\nrounds: 5"
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
          "id": "db71da47c15de30aa1f6b6eda5f03490442c27fa",
          "message": "Pre-allocate data file for performance",
          "timestamp": "2026-02-19T14:01:53+01:00",
          "tree_id": "ed3c8be25739cd19885da11a81f6aecb770ef3e3",
          "url": "https://github.com/uroni/hs5/commit/db71da47c15de30aa1f6b6eda5f03490442c27fa"
        },
        "date": 1771506695322,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07104607691407122,
            "unit": "iter/sec",
            "range": "stddev: 0.1490111823174303",
            "extra": "mean: 14.075372539 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06390911309516975,
            "unit": "iter/sec",
            "range": "stddev: 0.14863866806782988",
            "extra": "mean: 15.647220741599995 sec\nrounds: 5"
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
          "id": "ab6ca0e5fb4c4ce3104d3e50ac30db20561bfe37",
          "message": "Increase http read buffer size and make it configurable",
          "timestamp": "2026-02-19T23:02:00+01:00",
          "tree_id": "afcc11b3065dca32c89280dcbc52dd5ff7824130",
          "url": "https://github.com/uroni/hs5/commit/ab6ca0e5fb4c4ce3104d3e50ac30db20561bfe37"
        },
        "date": 1771539170115,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07326900474960511,
            "unit": "iter/sec",
            "range": "stddev: 0.1375521934190582",
            "extra": "mean: 13.648336065400008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06583156071568827,
            "unit": "iter/sec",
            "range": "stddev: 0.06313918168895712",
            "extra": "mean: 15.190282428800003 sec\nrounds: 5"
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
          "id": "90b909bcdcee9f54c8897bffb8bbc1ae228d6c07",
          "message": "Add WAL mode that logs only small object data",
          "timestamp": "2026-02-20T09:45:54+01:00",
          "tree_id": "5b844a0dba9f5034d9070d80d09d1ccf3fc06813",
          "url": "https://github.com/uroni/hs5/commit/90b909bcdcee9f54c8897bffb8bbc1ae228d6c07"
        },
        "date": 1771577704343,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0831042307644599,
            "unit": "iter/sec",
            "range": "stddev: 0.030801341333378952",
            "extra": "mean: 12.033081719199993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06748462726624609,
            "unit": "iter/sec",
            "range": "stddev: 0.08817915515863382",
            "extra": "mean: 14.818189571599987 sec\nrounds: 5"
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
          "id": "495c3815bc5b9d24f1308552edb04183ae59d8e9",
          "message": "Update performance comparison table",
          "timestamp": "2026-02-20T10:41:40+01:00",
          "tree_id": "6bda9512e97546b09cd44caff89a940b9873a3d8",
          "url": "https://github.com/uroni/hs5/commit/495c3815bc5b9d24f1308552edb04183ae59d8e9"
        },
        "date": 1771581054054,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09555194388623824,
            "unit": "iter/sec",
            "range": "stddev: 0.021147683812529613",
            "extra": "mean: 10.465511839199996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07760895154377451,
            "unit": "iter/sec",
            "range": "stddev: 0.11342635996904962",
            "extra": "mean: 12.88511157680001 sec\nrounds: 5"
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
          "id": "5bd6da6ca0dc2e5b2573027fa1145af4fe5c2a74",
          "message": "Rename param to wal-small-object-limit",
          "timestamp": "2026-02-20T10:56:23+01:00",
          "tree_id": "6da17891c36640663f55cb10dc6d6cf3ccaa3f31",
          "url": "https://github.com/uroni/hs5/commit/5bd6da6ca0dc2e5b2573027fa1145af4fe5c2a74"
        },
        "date": 1771581939137,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07724931413724453,
            "unit": "iter/sec",
            "range": "stddev: 0.1513610617804615",
            "extra": "mean: 12.945098751599994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06102999564013725,
            "unit": "iter/sec",
            "range": "stddev: 0.6983774291048",
            "extra": "mean: 16.38538540779996 sec\nrounds: 5"
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
          "id": "5bd6da6ca0dc2e5b2573027fa1145af4fe5c2a74",
          "message": "Rename param to wal-small-object-limit",
          "timestamp": "2026-02-20T10:56:23+01:00",
          "tree_id": "6da17891c36640663f55cb10dc6d6cf3ccaa3f31",
          "url": "https://github.com/uroni/hs5/commit/5bd6da6ca0dc2e5b2573027fa1145af4fe5c2a74"
        },
        "date": 1771587536570,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08389307884189154,
            "unit": "iter/sec",
            "range": "stddev: 0.048872838550285055",
            "extra": "mean: 11.919934442800013 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06693289693345833,
            "unit": "iter/sec",
            "range": "stddev: 0.1208409125839603",
            "extra": "mean: 14.940336453600015 sec\nrounds: 5"
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
          "id": "f494051e936495557cfd2750c6d5f7170a376d0c",
          "message": "Add benchmarks",
          "timestamp": "2026-02-20T12:38:08+01:00",
          "tree_id": "54edb81dc24eb10d691361132d1ca1f6455dfe86",
          "url": "https://github.com/uroni/hs5/commit/f494051e936495557cfd2750c6d5f7170a376d0c"
        },
        "date": 1771588028469,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08704194691139656,
            "unit": "iter/sec",
            "range": "stddev: 0.11740980731125585",
            "extra": "mean: 11.488713608599994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07112262351333419,
            "unit": "iter/sec",
            "range": "stddev: 0.3238543639907737",
            "extra": "mean: 14.060223746000009 sec\nrounds: 5"
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
          "id": "5d08b3000acce0fbf8c455b58eb5c3db19a7ff4e",
          "message": "Increment version",
          "timestamp": "2026-02-20T12:55:35+01:00",
          "tree_id": "106fbddbd3d53b083b14e121fba1801c510e6a5f",
          "url": "https://github.com/uroni/hs5/commit/5d08b3000acce0fbf8c455b58eb5c3db19a7ff4e"
        },
        "date": 1771589084822,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07839436258888696,
            "unit": "iter/sec",
            "range": "stddev: 0.13922007109751605",
            "extra": "mean: 12.756019272000026 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0600478528054758,
            "unit": "iter/sec",
            "range": "stddev: 0.569359246864979",
            "extra": "mean: 16.65338481360002 sec\nrounds: 5"
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
          "id": "5d08b3000acce0fbf8c455b58eb5c3db19a7ff4e",
          "message": "Increment version",
          "timestamp": "2026-02-20T12:55:35+01:00",
          "tree_id": "106fbddbd3d53b083b14e121fba1801c510e6a5f",
          "url": "https://github.com/uroni/hs5/commit/5d08b3000acce0fbf8c455b58eb5c3db19a7ff4e"
        },
        "date": 1771589141288,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08238219950596741,
            "unit": "iter/sec",
            "range": "stddev: 0.07847075350044772",
            "extra": "mean: 12.138544564199993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06630827895939745,
            "unit": "iter/sec",
            "range": "stddev: 0.13140417061724718",
            "extra": "mean: 15.081073068 sec\nrounds: 5"
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
          "id": "80bb2af78be08ce6fbb08bf634ecfe5171347407",
          "message": "Increment version",
          "timestamp": "2026-02-20T13:56:20+01:00",
          "tree_id": "f3e6350ed6902b34fe4218f2f2a5b642d9d598c7",
          "url": "https://github.com/uroni/hs5/commit/80bb2af78be08ce6fbb08bf634ecfe5171347407"
        },
        "date": 1771592715415,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08585184395941413,
            "unit": "iter/sec",
            "range": "stddev: 0.08838702670529903",
            "extra": "mean: 11.647973460800017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06885563750242835,
            "unit": "iter/sec",
            "range": "stddev: 0.03151791694191645",
            "extra": "mean: 14.523139081600004 sec\nrounds: 5"
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
          "id": "80bb2af78be08ce6fbb08bf634ecfe5171347407",
          "message": "Increment version",
          "timestamp": "2026-02-20T13:56:20+01:00",
          "tree_id": "f3e6350ed6902b34fe4218f2f2a5b642d9d598c7",
          "url": "https://github.com/uroni/hs5/commit/80bb2af78be08ce6fbb08bf634ecfe5171347407"
        },
        "date": 1771592733834,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07825971275518646,
            "unit": "iter/sec",
            "range": "stddev: 0.15709897375569662",
            "extra": "mean: 12.777966654800014 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0617419477676099,
            "unit": "iter/sec",
            "range": "stddev: 0.17976600664012227",
            "extra": "mean: 16.196444008600007 sec\nrounds: 5"
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
          "id": "94b737b448c16ecf3d572c3bd0c90ff5b4dd4ff4",
          "message": "Increment version",
          "timestamp": "2026-02-20T14:55:32+01:00",
          "tree_id": "a981bbf2b40809438bf6c24afa4a1f1662d49f25",
          "url": "https://github.com/uroni/hs5/commit/94b737b448c16ecf3d572c3bd0c90ff5b4dd4ff4"
        },
        "date": 1771596274759,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08231951788794356,
            "unit": "iter/sec",
            "range": "stddev: 0.08149026267169054",
            "extra": "mean: 12.1477873736 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06644855786417517,
            "unit": "iter/sec",
            "range": "stddev: 0.052905170671463594",
            "extra": "mean: 15.049235561199987 sec\nrounds: 5"
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
          "id": "94b737b448c16ecf3d572c3bd0c90ff5b4dd4ff4",
          "message": "Increment version",
          "timestamp": "2026-02-20T14:55:32+01:00",
          "tree_id": "a981bbf2b40809438bf6c24afa4a1f1662d49f25",
          "url": "https://github.com/uroni/hs5/commit/94b737b448c16ecf3d572c3bd0c90ff5b4dd4ff4"
        },
        "date": 1771596320381,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0794404037799615,
            "unit": "iter/sec",
            "range": "stddev: 0.19845027401709425",
            "extra": "mean: 12.588052834799987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0634515624660982,
            "unit": "iter/sec",
            "range": "stddev: 0.10968672265051205",
            "extra": "mean: 15.76005319860002 sec\nrounds: 5"
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
          "id": "73a30d4a41ba80ea988dfeaf8b893832f5ebc1a3",
          "message": "Implement chunked upload support",
          "timestamp": "2026-02-21T16:32:40+01:00",
          "tree_id": "b00251842f564a0c89e0b8a12afa3b8ee882c37e",
          "url": "https://github.com/uroni/hs5/commit/73a30d4a41ba80ea988dfeaf8b893832f5ebc1a3"
        },
        "date": 1771688517205,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08373112004177204,
            "unit": "iter/sec",
            "range": "stddev: 0.09174615500262193",
            "extra": "mean: 11.942990843799976 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0648114618102809,
            "unit": "iter/sec",
            "range": "stddev: 0.9110699737120295",
            "extra": "mean: 15.429369621800015 sec\nrounds: 5"
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
          "id": "73a30d4a41ba80ea988dfeaf8b893832f5ebc1a3",
          "message": "Implement chunked upload support",
          "timestamp": "2026-02-21T16:32:40+01:00",
          "tree_id": "b00251842f564a0c89e0b8a12afa3b8ee882c37e",
          "url": "https://github.com/uroni/hs5/commit/73a30d4a41ba80ea988dfeaf8b893832f5ebc1a3"
        },
        "date": 1771689726492,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08621629613624816,
            "unit": "iter/sec",
            "range": "stddev: 0.10607913141567651",
            "extra": "mean: 11.59873532979999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06939366377758598,
            "unit": "iter/sec",
            "range": "stddev: 0.29780995363057394",
            "extra": "mean: 14.410537584600025 sec\nrounds: 5"
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
          "id": "102906d12650a23d77a1599a8108e8f45291225e",
          "message": "Increment version",
          "timestamp": "2026-02-21T17:58:53+01:00",
          "tree_id": "a3bddca309a5ea11e188f3c29e3b88d55e9fb2cb",
          "url": "https://github.com/uroni/hs5/commit/102906d12650a23d77a1599a8108e8f45291225e"
        },
        "date": 1771693630743,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0957280778675408,
            "unit": "iter/sec",
            "range": "stddev: 0.13732644659644644",
            "extra": "mean: 10.446255918600002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0660967247772328,
            "unit": "iter/sec",
            "range": "stddev: 0.12518550290371872",
            "extra": "mean: 15.129342692399984 sec\nrounds: 5"
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
          "id": "949edf558577fb7042cf1a7738e0bb604cf00ff9",
          "message": "Don't return manual commit object if it is disabled",
          "timestamp": "2026-02-21T19:45:57+01:00",
          "tree_id": "a11802141b3a1126fac1543d13d7418302867e30",
          "url": "https://github.com/uroni/hs5/commit/949edf558577fb7042cf1a7738e0bb604cf00ff9"
        },
        "date": 1771700374851,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08175404849460165,
            "unit": "iter/sec",
            "range": "stddev: 0.04177523556336877",
            "extra": "mean: 12.23181014779998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06604559702431397,
            "unit": "iter/sec",
            "range": "stddev: 0.11222712182982855",
            "extra": "mean: 15.141054741799985 sec\nrounds: 5"
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
          "id": "0e1c38cbcf821c8f15ce8f9f820dfc3a4dc14920",
          "message": "Fix manual commit object test",
          "timestamp": "2026-02-21T20:14:30+01:00",
          "tree_id": "c5e085c4e92dce224657faca2cf8406bacd5e601",
          "url": "https://github.com/uroni/hs5/commit/0e1c38cbcf821c8f15ce8f9f820dfc3a4dc14920"
        },
        "date": 1771701806288,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08265796427038727,
            "unit": "iter/sec",
            "range": "stddev: 0.1311111151109171",
            "extra": "mean: 12.098047766200022 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05867666870494543,
            "unit": "iter/sec",
            "range": "stddev: 0.39075402185285807",
            "extra": "mean: 17.042548973400006 sec\nrounds: 5"
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
          "id": "0e1c38cbcf821c8f15ce8f9f820dfc3a4dc14920",
          "message": "Fix manual commit object test",
          "timestamp": "2026-02-21T20:14:30+01:00",
          "tree_id": "c5e085c4e92dce224657faca2cf8406bacd5e601",
          "url": "https://github.com/uroni/hs5/commit/0e1c38cbcf821c8f15ce8f9f820dfc3a4dc14920"
        },
        "date": 1771702394446,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08329846016920513,
            "unit": "iter/sec",
            "range": "stddev: 0.02901270045420445",
            "extra": "mean: 12.005023837999989 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06593452905558966,
            "unit": "iter/sec",
            "range": "stddev: 0.0696274371613007",
            "extra": "mean: 15.166560136599992 sec\nrounds: 5"
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
          "id": "ea66ba9649eee7fb55ceee2a762fcf375b483407",
          "message": "Increment version",
          "timestamp": "2026-02-22T13:24:21+01:00",
          "tree_id": "b67b75b4305821e45dd7a028b54c1d4a771fb60a",
          "url": "https://github.com/uroni/hs5/commit/ea66ba9649eee7fb55ceee2a762fcf375b483407"
        },
        "date": 1771763586717,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0877353337002801,
            "unit": "iter/sec",
            "range": "stddev: 0.16886761730345445",
            "extra": "mean: 11.397916413199983 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07058552220554219,
            "unit": "iter/sec",
            "range": "stddev: 0.0495795326353191",
            "extra": "mean: 14.167211189400007 sec\nrounds: 5"
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
          "id": "ea66ba9649eee7fb55ceee2a762fcf375b483407",
          "message": "Increment version",
          "timestamp": "2026-02-22T13:24:21+01:00",
          "tree_id": "b67b75b4305821e45dd7a028b54c1d4a771fb60a",
          "url": "https://github.com/uroni/hs5/commit/ea66ba9649eee7fb55ceee2a762fcf375b483407"
        },
        "date": 1771765238476,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07894591854934213,
            "unit": "iter/sec",
            "range": "stddev: 0.15386317866781743",
            "extra": "mean: 12.666899294799999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06366331841764844,
            "unit": "iter/sec",
            "range": "stddev: 0.1446983240672513",
            "extra": "mean: 15.70763235180001 sec\nrounds: 5"
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
          "id": "87da196135081607c7068955192bc0d2b310f6ad",
          "message": "Fix for older Python version",
          "timestamp": "2026-02-22T22:02:50+01:00",
          "tree_id": "74d1a9696abcde708613238d8d9367eaf8886f98",
          "url": "https://github.com/uroni/hs5/commit/87da196135081607c7068955192bc0d2b310f6ad"
        },
        "date": 1771794702978,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08414064388679392,
            "unit": "iter/sec",
            "range": "stddev: 0.06168297919096419",
            "extra": "mean: 11.884862699000006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06695184223348918,
            "unit": "iter/sec",
            "range": "stddev: 0.029510883753506967",
            "extra": "mean: 14.936108800599992 sec\nrounds: 5"
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
          "id": "87da196135081607c7068955192bc0d2b310f6ad",
          "message": "Fix for older Python version",
          "timestamp": "2026-02-22T22:02:50+01:00",
          "tree_id": "74d1a9696abcde708613238d8d9367eaf8886f98",
          "url": "https://github.com/uroni/hs5/commit/87da196135081607c7068955192bc0d2b310f6ad"
        },
        "date": 1771795387706,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08010086536014123,
            "unit": "iter/sec",
            "range": "stddev: 0.15511527382529539",
            "extra": "mean: 12.484259633200008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06394903643075972,
            "unit": "iter/sec",
            "range": "stddev: 0.165203496763408",
            "extra": "mean: 15.637452193399998 sec\nrounds: 5"
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
          "id": "f9b3e04a6edd7024680f08e5616b269261bfc10e",
          "message": "Add comparison with garage",
          "timestamp": "2026-02-24T18:20:20+01:00",
          "tree_id": "4871c39b6770ef7e32c560c30fa6d91cac1ce8fc",
          "url": "https://github.com/uroni/hs5/commit/f9b3e04a6edd7024680f08e5616b269261bfc10e"
        },
        "date": 1771954261247,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08283429780386872,
            "unit": "iter/sec",
            "range": "stddev: 0.06220421922988491",
            "extra": "mean: 12.072294043800003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06553858635227076,
            "unit": "iter/sec",
            "range": "stddev: 0.10540159498061076",
            "extra": "mean: 15.258186904200022 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06091378963761256,
            "unit": "iter/sec",
            "range": "stddev: 0.2127428915770518",
            "extra": "mean: 16.416644013599967 sec\nrounds: 5"
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
          "id": "49f4b639ec381d0c7867d7859ef2c9e41fc4a34b",
          "message": "Add additional note about Garage in performance section",
          "timestamp": "2026-02-24T20:29:21+01:00",
          "tree_id": "996067d3f39e08909c0642c2fda04295f23d23ab",
          "url": "https://github.com/uroni/hs5/commit/49f4b639ec381d0c7867d7859ef2c9e41fc4a34b"
        },
        "date": 1771962006720,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0789666524582853,
            "unit": "iter/sec",
            "range": "stddev: 0.29180768760190146",
            "extra": "mean: 12.663573405600005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.062495459489258706,
            "unit": "iter/sec",
            "range": "stddev: 0.18967103648632638",
            "extra": "mean: 16.001162455200017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05768032621559236,
            "unit": "iter/sec",
            "range": "stddev: 0.4713168881151758",
            "extra": "mean: 17.336933849199976 sec\nrounds: 5"
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
          "id": "57d7fd6d6fe1b677f6962e84e08dbf44406e85a6",
          "message": "Implement conditional headers\n\nIf-match etc. on PUT, GET and DELETE",
          "timestamp": "2026-02-25T13:09:06+01:00",
          "tree_id": "583382840f695ead58284073d209cb504ce8287e",
          "url": "https://github.com/uroni/hs5/commit/57d7fd6d6fe1b677f6962e84e08dbf44406e85a6"
        },
        "date": 1772021971877,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08299610704532291,
            "unit": "iter/sec",
            "range": "stddev: 0.08667640033572124",
            "extra": "mean: 12.048757894799996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06712804305805227,
            "unit": "iter/sec",
            "range": "stddev: 0.0407572831151672",
            "extra": "mean: 14.896903804199997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06215106214771713,
            "unit": "iter/sec",
            "range": "stddev: 0.14444236066775032",
            "extra": "mean: 16.08982960940002 sec\nrounds: 5"
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
          "id": "7eb057bc6e06509c1153573a1034a7f29cedddfe",
          "message": "Implement conditional multi-part upload completion",
          "timestamp": "2026-02-25T14:18:34+01:00",
          "tree_id": "e5d175035b206666f80c4b7cd9b350b515223f38",
          "url": "https://github.com/uroni/hs5/commit/7eb057bc6e06509c1153573a1034a7f29cedddfe"
        },
        "date": 1772026113834,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08430034246906556,
            "unit": "iter/sec",
            "range": "stddev: 0.13403722068621085",
            "extra": "mean: 11.862348013200005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06747012545269501,
            "unit": "iter/sec",
            "range": "stddev: 0.04051718673109891",
            "extra": "mean: 14.821374545999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.0624472838108556,
            "unit": "iter/sec",
            "range": "stddev: 0.12322276067902574",
            "extra": "mean: 16.013506736800036 sec\nrounds: 5"
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
          "id": "7eb057bc6e06509c1153573a1034a7f29cedddfe",
          "message": "Implement conditional multi-part upload completion",
          "timestamp": "2026-02-25T14:18:34+01:00",
          "tree_id": "e5d175035b206666f80c4b7cd9b350b515223f38",
          "url": "https://github.com/uroni/hs5/commit/7eb057bc6e06509c1153573a1034a7f29cedddfe"
        },
        "date": 1772027276056,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08372677534999869,
            "unit": "iter/sec",
            "range": "stddev: 0.12889386458790397",
            "extra": "mean: 11.943610581200005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06731610142084184,
            "unit": "iter/sec",
            "range": "stddev: 0.11573659749076418",
            "extra": "mean: 14.855286906000003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06180733565255119,
            "unit": "iter/sec",
            "range": "stddev: 0.1916260642521571",
            "extra": "mean: 16.1793092914 sec\nrounds: 5"
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
          "id": "b07bd43dc0a2032f948f0e7bd52934af9ec90534",
          "message": "Return Bucket creation date and region",
          "timestamp": "2026-02-26T11:52:39+01:00",
          "tree_id": "f9813cd8a225b87f073a592dec707af6e765ce74",
          "url": "https://github.com/uroni/hs5/commit/b07bd43dc0a2032f948f0e7bd52934af9ec90534"
        },
        "date": 1772103847292,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07755833257597562,
            "unit": "iter/sec",
            "range": "stddev: 0.30860080260106626",
            "extra": "mean: 12.893521131599972 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06243348159715635,
            "unit": "iter/sec",
            "range": "stddev: 0.07530987490024449",
            "extra": "mean: 16.017046854 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.053818434392924254,
            "unit": "iter/sec",
            "range": "stddev: 0.24972367012931496",
            "extra": "mean: 18.580993878400044 sec\nrounds: 5"
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
          "id": "b07bd43dc0a2032f948f0e7bd52934af9ec90534",
          "message": "Return Bucket creation date and region",
          "timestamp": "2026-02-26T11:52:39+01:00",
          "tree_id": "f9813cd8a225b87f073a592dec707af6e765ce74",
          "url": "https://github.com/uroni/hs5/commit/b07bd43dc0a2032f948f0e7bd52934af9ec90534"
        },
        "date": 1772104596364,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07847843493512958,
            "unit": "iter/sec",
            "range": "stddev: 0.09887633207313826",
            "extra": "mean: 12.742354008800021 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06297470695974658,
            "unit": "iter/sec",
            "range": "stddev: 0.15322431001741887",
            "extra": "mean: 15.879391080600021 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.055658123809301685,
            "unit": "iter/sec",
            "range": "stddev: 0.10589814209133994",
            "extra": "mean: 17.96682912679996 sec\nrounds: 5"
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
          "id": "168f56cbe323dc2bfe6379ec4761b845f1cb9a09",
          "message": "Implement CopyObject and UploadPartCopy",
          "timestamp": "2026-02-26T18:41:33+01:00",
          "tree_id": "4fab83b72466d50b1320e7b34024cb107bf28be6",
          "url": "https://github.com/uroni/hs5/commit/168f56cbe323dc2bfe6379ec4761b845f1cb9a09"
        },
        "date": 1772128272826,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09810230118087372,
            "unit": "iter/sec",
            "range": "stddev: 0.05806839465820517",
            "extra": "mean: 10.193440805800003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07908971166771325,
            "unit": "iter/sec",
            "range": "stddev: 0.09462757914242532",
            "extra": "mean: 12.643869587000017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.07058982637385561,
            "unit": "iter/sec",
            "range": "stddev: 0.2067232661505105",
            "extra": "mean: 14.166347353000015 sec\nrounds: 5"
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
          "id": "168f56cbe323dc2bfe6379ec4761b845f1cb9a09",
          "message": "Implement CopyObject and UploadPartCopy",
          "timestamp": "2026-02-26T18:41:33+01:00",
          "tree_id": "4fab83b72466d50b1320e7b34024cb107bf28be6",
          "url": "https://github.com/uroni/hs5/commit/168f56cbe323dc2bfe6379ec4761b845f1cb9a09"
        },
        "date": 1772129127937,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08204323585118195,
            "unit": "iter/sec",
            "range": "stddev: 0.06005753658238689",
            "extra": "mean: 12.188695260800023 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06593037042349033,
            "unit": "iter/sec",
            "range": "stddev: 0.06561939211455305",
            "extra": "mean: 15.167516784400016 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.057166427441048585,
            "unit": "iter/sec",
            "range": "stddev: 0.14313230470335372",
            "extra": "mean: 17.492784572400023 sec\nrounds: 5"
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
          "id": "cf59564f34c5a8b288d2cf003faca97e2af2964e",
          "message": "Trim large continuous used space from data file\n\nIf there are large extents (default > 1MiB) in the data file that are free, trim them via hole punching, to make the space available to other applications.",
          "timestamp": "2026-02-26T21:45:03+01:00",
          "tree_id": "533c1358bfd1f8140b9a3e9b59034bd12d3e9c2d",
          "url": "https://github.com/uroni/hs5/commit/cf59564f34c5a8b288d2cf003faca97e2af2964e"
        },
        "date": 1772139429035,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07178719578440419,
            "unit": "iter/sec",
            "range": "stddev: 0.7949733781244371",
            "extra": "mean: 13.930060773000003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06119924231977445,
            "unit": "iter/sec",
            "range": "stddev: 0.14378056694257577",
            "extra": "mean: 16.340071577599975 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05021786491497498,
            "unit": "iter/sec",
            "range": "stddev: 0.17848487559240575",
            "extra": "mean: 19.913232107600017 sec\nrounds: 5"
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
          "id": "cf59564f34c5a8b288d2cf003faca97e2af2964e",
          "message": "Trim large continuous used space from data file\n\nIf there are large extents (default > 1MiB) in the data file that are free, trim them via hole punching, to make the space available to other applications.",
          "timestamp": "2026-02-26T21:45:03+01:00",
          "tree_id": "533c1358bfd1f8140b9a3e9b59034bd12d3e9c2d",
          "url": "https://github.com/uroni/hs5/commit/cf59564f34c5a8b288d2cf003faca97e2af2964e"
        },
        "date": 1772140308696,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08672233703430478,
            "unit": "iter/sec",
            "range": "stddev: 0.03188685307974031",
            "extra": "mean: 11.531054561000008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06937362878361672,
            "unit": "iter/sec",
            "range": "stddev: 0.1081015285118494",
            "extra": "mean: 14.414699324999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06006459939905024,
            "unit": "iter/sec",
            "range": "stddev: 0.1200931259657992",
            "extra": "mean: 16.648741688199994 sec\nrounds: 5"
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
          "id": "17594dd24c13ccadf483563d8601003996046f36",
          "message": "Fix x-id check",
          "timestamp": "2026-02-26T23:26:41+01:00",
          "tree_id": "f1ad59d437dc2cedb23d927f6cfa43596273457c",
          "url": "https://github.com/uroni/hs5/commit/17594dd24c13ccadf483563d8601003996046f36"
        },
        "date": 1772145408936,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08126332327066373,
            "unit": "iter/sec",
            "range": "stddev: 0.053780366111460934",
            "extra": "mean: 12.305674438999995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0650872619326482,
            "unit": "iter/sec",
            "range": "stddev: 0.14221188416891756",
            "extra": "mean: 15.363989362999973 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.056064212815144655,
            "unit": "iter/sec",
            "range": "stddev: 0.15148120539345214",
            "extra": "mean: 17.83669028400002 sec\nrounds: 5"
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
          "id": "c6966938aef8379b3cbd949b3faeb88b28e14d2e",
          "message": "Increment version",
          "timestamp": "2026-02-26T23:27:18+01:00",
          "tree_id": "75f708fe939b1c14c855230d1f2f826b8e64557a",
          "url": "https://github.com/uroni/hs5/commit/c6966938aef8379b3cbd949b3faeb88b28e14d2e"
        },
        "date": 1772145414178,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09085289073644585,
            "unit": "iter/sec",
            "range": "stddev: 0.3696746231099431",
            "extra": "mean: 11.006804427399993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07430681493837499,
            "unit": "iter/sec",
            "range": "stddev: 0.09115195734952931",
            "extra": "mean: 13.457715834400005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06642613096221238,
            "unit": "iter/sec",
            "range": "stddev: 0.08103308207333458",
            "extra": "mean: 15.054316509399996 sec\nrounds: 5"
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
          "id": "c6966938aef8379b3cbd949b3faeb88b28e14d2e",
          "message": "Increment version",
          "timestamp": "2026-02-26T23:27:18+01:00",
          "tree_id": "75f708fe939b1c14c855230d1f2f826b8e64557a",
          "url": "https://github.com/uroni/hs5/commit/c6966938aef8379b3cbd949b3faeb88b28e14d2e"
        },
        "date": 1772145465536,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09491716059524079,
            "unit": "iter/sec",
            "range": "stddev: 0.04129878813761406",
            "extra": "mean: 10.535502681799993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07559596745774054,
            "unit": "iter/sec",
            "range": "stddev: 0.22906657323804347",
            "extra": "mean: 13.228218827399985 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06632878450075534,
            "unit": "iter/sec",
            "range": "stddev: 0.1200742033187455",
            "extra": "mean: 15.076410754800008 sec\nrounds: 5"
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
          "id": "689f5eecb076726b194d7b4e50d6d012e09ead63",
          "message": "Verify etags in CompleteMultipartUpload",
          "timestamp": "2026-02-27T00:03:22+01:00",
          "tree_id": "0e2efc64c0b46a4d8002f68a07e290d61157e416",
          "url": "https://github.com/uroni/hs5/commit/689f5eecb076726b194d7b4e50d6d012e09ead63"
        },
        "date": 1772147696474,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07694460667983068,
            "unit": "iter/sec",
            "range": "stddev: 0.10217851221318132",
            "extra": "mean: 12.99636248920001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06194631609890505,
            "unit": "iter/sec",
            "range": "stddev: 0.1038433978113781",
            "extra": "mean: 16.143009996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05446160724010002,
            "unit": "iter/sec",
            "range": "stddev: 0.24164751413205685",
            "extra": "mean: 18.361558732400045 sec\nrounds: 5"
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
          "id": "689f5eecb076726b194d7b4e50d6d012e09ead63",
          "message": "Verify etags in CompleteMultipartUpload",
          "timestamp": "2026-02-27T00:03:22+01:00",
          "tree_id": "0e2efc64c0b46a4d8002f68a07e290d61157e416",
          "url": "https://github.com/uroni/hs5/commit/689f5eecb076726b194d7b4e50d6d012e09ead63"
        },
        "date": 1772148869274,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08092768583937363,
            "unit": "iter/sec",
            "range": "stddev: 0.04969645975131448",
            "extra": "mean: 12.356710680999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06516701913826446,
            "unit": "iter/sec",
            "range": "stddev: 0.10467078196120469",
            "extra": "mean: 15.345185543599996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05558838600630119,
            "unit": "iter/sec",
            "range": "stddev: 0.10631761957172794",
            "extra": "mean: 17.989369216200043 sec\nrounds: 5"
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
          "id": "85a9b7cf7be2691bb7a48d719a31242ce89b260f",
          "message": "Fix continuation token in combination with delimiter",
          "timestamp": "2026-02-27T11:12:29+01:00",
          "tree_id": "7b16904e5d6a3c799fbacaa63ea131616683f1bf",
          "url": "https://github.com/uroni/hs5/commit/85a9b7cf7be2691bb7a48d719a31242ce89b260f"
        },
        "date": 1772187947085,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06785745607979267,
            "unit": "iter/sec",
            "range": "stddev: 0.3037224712184232",
            "extra": "mean: 14.736774081600014 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.052959076847844566,
            "unit": "iter/sec",
            "range": "stddev: 0.5866864675269235",
            "extra": "mean: 18.882504369800017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.04590864192283587,
            "unit": "iter/sec",
            "range": "stddev: 0.44257594366297653",
            "extra": "mean: 21.782391247399982 sec\nrounds: 5"
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
          "id": "85a9b7cf7be2691bb7a48d719a31242ce89b260f",
          "message": "Fix continuation token in combination with delimiter",
          "timestamp": "2026-02-27T11:12:29+01:00",
          "tree_id": "7b16904e5d6a3c799fbacaa63ea131616683f1bf",
          "url": "https://github.com/uroni/hs5/commit/85a9b7cf7be2691bb7a48d719a31242ce89b260f"
        },
        "date": 1772188675399,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07501409853937778,
            "unit": "iter/sec",
            "range": "stddev: 0.11323708383558288",
            "extra": "mean: 13.330827397399991 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.059598360873613886,
            "unit": "iter/sec",
            "range": "stddev: 0.22604692380137842",
            "extra": "mean: 16.778984947599998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05162082044705092,
            "unit": "iter/sec",
            "range": "stddev: 0.7161033311384295",
            "extra": "mean: 19.372028405200012 sec\nrounds: 5"
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
          "id": "e4c995235f7abde8c98a7ed82089b795d409e0f0",
          "message": "Increment version",
          "timestamp": "2026-02-27T14:20:50+01:00",
          "tree_id": "185705d37e7d131fbd86d3dafb22d65a6c1bd1e9",
          "url": "https://github.com/uroni/hs5/commit/e4c995235f7abde8c98a7ed82089b795d409e0f0"
        },
        "date": 1772199108046,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07773437442256706,
            "unit": "iter/sec",
            "range": "stddev: 0.2547002836149805",
            "extra": "mean: 12.864321703600023 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06310110497356804,
            "unit": "iter/sec",
            "range": "stddev: 0.261351189716806",
            "extra": "mean: 15.847583024400013 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.053500784890974165,
            "unit": "iter/sec",
            "range": "stddev: 0.3694756382842023",
            "extra": "mean: 18.691314567400013 sec\nrounds: 5"
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
          "id": "e4c995235f7abde8c98a7ed82089b795d409e0f0",
          "message": "Increment version",
          "timestamp": "2026-02-27T14:20:50+01:00",
          "tree_id": "185705d37e7d131fbd86d3dafb22d65a6c1bd1e9",
          "url": "https://github.com/uroni/hs5/commit/e4c995235f7abde8c98a7ed82089b795d409e0f0"
        },
        "date": 1772199109919,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08315876799984513,
            "unit": "iter/sec",
            "range": "stddev: 0.11500822847051989",
            "extra": "mean: 12.02519017599999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0662636110342349,
            "unit": "iter/sec",
            "range": "stddev: 0.24781703198912589",
            "extra": "mean: 15.091239134 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05687129755497624,
            "unit": "iter/sec",
            "range": "stddev: 0.06140328178743948",
            "extra": "mean: 17.583562236000013 sec\nrounds: 5"
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
          "id": "ab2075e6ade0fbe70ff692a7664b2949f1d44c05",
          "message": "Set default wal path correctly\n\nIf empty, wal won't be enabled",
          "timestamp": "2026-02-27T16:12:17+01:00",
          "tree_id": "05eae38905efb7f4f7c691060f0bbe26b26d96dc",
          "url": "https://github.com/uroni/hs5/commit/ab2075e6ade0fbe70ff692a7664b2949f1d44c05"
        },
        "date": 1772207481709,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07565773711987486,
            "unit": "iter/sec",
            "range": "stddev: 0.28220211176165433",
            "extra": "mean: 13.217418839999983 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06108806966063482,
            "unit": "iter/sec",
            "range": "stddev: 0.4444000951546995",
            "extra": "mean: 16.369808467600024 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05445793577471927,
            "unit": "iter/sec",
            "range": "stddev: 0.3894150853509989",
            "extra": "mean: 18.362796638799978 sec\nrounds: 5"
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
          "id": "a80bee5d5a94fe4727fa2089e867ed7f23a9f6a6",
          "message": "Update AWS explorer",
          "timestamp": "2026-03-04T12:17:31+01:00",
          "tree_id": "80c8956846af87eaaa47bf92144186e6d07fa12d",
          "url": "https://github.com/uroni/hs5/commit/a80bee5d5a94fe4727fa2089e867ed7f23a9f6a6"
        },
        "date": 1772623735499,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07927990041212939,
            "unit": "iter/sec",
            "range": "stddev: 0.07043315773881659",
            "extra": "mean: 12.613537539800006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06375724641875125,
            "unit": "iter/sec",
            "range": "stddev: 0.05888607431566988",
            "extra": "mean: 15.68449166439999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05465362666501894,
            "unit": "iter/sec",
            "range": "stddev: 0.11131557131753207",
            "extra": "mean: 18.2970474426 sec\nrounds: 5"
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
          "id": "a80bee5d5a94fe4727fa2089e867ed7f23a9f6a6",
          "message": "Update AWS explorer",
          "timestamp": "2026-03-04T12:17:31+01:00",
          "tree_id": "80c8956846af87eaaa47bf92144186e6d07fa12d",
          "url": "https://github.com/uroni/hs5/commit/a80bee5d5a94fe4727fa2089e867ed7f23a9f6a6"
        },
        "date": 1772624122669,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.08020481757457122,
            "unit": "iter/sec",
            "range": "stddev: 0.07897270110962405",
            "extra": "mean: 12.468078978799998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06395339173651035,
            "unit": "iter/sec",
            "range": "stddev: 0.1790774870441468",
            "extra": "mean: 15.636387263399978 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05238825687246485,
            "unit": "iter/sec",
            "range": "stddev: 0.2115159525346295",
            "extra": "mean: 19.088247246599984 sec\nrounds: 5"
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
          "id": "ef69676581db6c2ba7b31561f204c3bc8a28088b",
          "message": "Increment version",
          "timestamp": "2026-03-04T13:18:47+01:00",
          "tree_id": "4717045cf3529b7fed8d818ce419631cc31ca3b7",
          "url": "https://github.com/uroni/hs5/commit/ef69676581db6c2ba7b31561f204c3bc8a28088b"
        },
        "date": 1772627435471,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07841831034894517,
            "unit": "iter/sec",
            "range": "stddev: 0.06319510039025814",
            "extra": "mean: 12.752123777600001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06221686416571905,
            "unit": "iter/sec",
            "range": "stddev: 0.24167168918452042",
            "extra": "mean: 16.072812627400005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.054847401181426995,
            "unit": "iter/sec",
            "range": "stddev: 0.28716789216133426",
            "extra": "mean: 18.232404425000006 sec\nrounds: 5"
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
          "id": "b1570e8d660032fc67fc1e1a198d83da4b6e08ad",
          "message": "Correctly handle the quiet param",
          "timestamp": "2026-03-04T13:24:42+01:00",
          "tree_id": "d9d5607ac5bb57f7aa72d4877b4fec0642922781",
          "url": "https://github.com/uroni/hs5/commit/b1570e8d660032fc67fc1e1a198d83da4b6e08ad"
        },
        "date": 1772628069418,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07602483905308972,
            "unit": "iter/sec",
            "range": "stddev: 0.05483632692945803",
            "extra": "mean: 13.153595751800003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.059491052640434285,
            "unit": "iter/sec",
            "range": "stddev: 0.13945435927750566",
            "extra": "mean: 16.8092503934 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.051495491213194675,
            "unit": "iter/sec",
            "range": "stddev: 0.4374875770830825",
            "extra": "mean: 19.419175862599992 sec\nrounds: 5"
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
          "id": "b1570e8d660032fc67fc1e1a198d83da4b6e08ad",
          "message": "Correctly handle the quiet param",
          "timestamp": "2026-03-04T13:24:42+01:00",
          "tree_id": "d9d5607ac5bb57f7aa72d4877b4fec0642922781",
          "url": "https://github.com/uroni/hs5/commit/b1570e8d660032fc67fc1e1a198d83da4b6e08ad"
        },
        "date": 1772628476752,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0883903058493938,
            "unit": "iter/sec",
            "range": "stddev: 0.12138365222946801",
            "extra": "mean: 11.313457854800015 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07071191499394472,
            "unit": "iter/sec",
            "range": "stddev: 0.27315471355993076",
            "extra": "mean: 14.14188825299998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06429902780606697,
            "unit": "iter/sec",
            "range": "stddev: 0.167303068236843",
            "extra": "mean: 15.552334679399996 sec\nrounds: 5"
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
          "id": "dee85f1bd71f830659bd1b229d1334a24ccd9c0d",
          "message": "Fix commit item leaks",
          "timestamp": "2026-03-04T15:34:16+01:00",
          "tree_id": "44d873d4d1c83e6ac03631e948982da3e1604c38",
          "url": "https://github.com/uroni/hs5/commit/dee85f1bd71f830659bd1b229d1334a24ccd9c0d"
        },
        "date": 1772635509552,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07827633316251471,
            "unit": "iter/sec",
            "range": "stddev: 0.146172463323783",
            "extra": "mean: 12.775253510199992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06379892024460637,
            "unit": "iter/sec",
            "range": "stddev: 0.1743925538577861",
            "extra": "mean: 15.674246463199996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05369488501220053,
            "unit": "iter/sec",
            "range": "stddev: 0.22121782678250645",
            "extra": "mean: 18.623747862999995 sec\nrounds: 5"
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
          "id": "ebff623a6324fe93e2a7d4e09334e965a7c1530e",
          "message": "Serialize linked items and match info to WAL",
          "timestamp": "2026-03-04T17:48:52+01:00",
          "tree_id": "03103726ebf9e7df1ea3a6254bf427ee2dea89e7",
          "url": "https://github.com/uroni/hs5/commit/ebff623a6324fe93e2a7d4e09334e965a7c1530e"
        },
        "date": 1772643653564,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07811862243350323,
            "unit": "iter/sec",
            "range": "stddev: 0.13590531845119289",
            "extra": "mean: 12.8010449858 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06263551111717224,
            "unit": "iter/sec",
            "range": "stddev: 0.15163112676992682",
            "extra": "mean: 15.965384207199975 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05351590425752365,
            "unit": "iter/sec",
            "range": "stddev: 0.6509447503032565",
            "extra": "mean: 18.686033878599982 sec\nrounds: 5"
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
          "id": "ebff623a6324fe93e2a7d4e09334e965a7c1530e",
          "message": "Serialize linked items and match info to WAL",
          "timestamp": "2026-03-04T17:48:52+01:00",
          "tree_id": "03103726ebf9e7df1ea3a6254bf427ee2dea89e7",
          "url": "https://github.com/uroni/hs5/commit/ebff623a6324fe93e2a7d4e09334e965a7c1530e"
        },
        "date": 1772644469931,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07924297774483598,
            "unit": "iter/sec",
            "range": "stddev: 0.06377418505990125",
            "extra": "mean: 12.619414722400018 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06296642917151202,
            "unit": "iter/sec",
            "range": "stddev: 0.4162256611356094",
            "extra": "mean: 15.881478641199987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05520383534714075,
            "unit": "iter/sec",
            "range": "stddev: 0.25889962995243004",
            "extra": "mean: 18.114683404000015 sec\nrounds: 5"
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
          "id": "ebff623a6324fe93e2a7d4e09334e965a7c1530e",
          "message": "Serialize linked items and match info to WAL",
          "timestamp": "2026-03-04T17:48:52+01:00",
          "tree_id": "03103726ebf9e7df1ea3a6254bf427ee2dea89e7",
          "url": "https://github.com/uroni/hs5/commit/ebff623a6324fe93e2a7d4e09334e965a7c1530e"
        },
        "date": 1772644472387,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.075125016262641,
            "unit": "iter/sec",
            "range": "stddev: 0.14097212907688098",
            "extra": "mean: 13.311145204999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05884897298955517,
            "unit": "iter/sec",
            "range": "stddev: 0.21773669214005728",
            "extra": "mean: 16.9926499852 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05119027921594928,
            "unit": "iter/sec",
            "range": "stddev: 0.15545225478305258",
            "extra": "mean: 19.534958888999995 sec\nrounds: 5"
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
          "id": "746a7d69df4e97c5458671649e9464f6d443d09f",
          "message": "Increment version",
          "timestamp": "2026-03-06T18:47:25+01:00",
          "tree_id": "e4a8cadb75c07c4fbd207269f7ec6e8ceb4b7fb5",
          "url": "https://github.com/uroni/hs5/commit/746a7d69df4e97c5458671649e9464f6d443d09f"
        },
        "date": 1772819898884,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07547055991691626,
            "unit": "iter/sec",
            "range": "stddev: 0.12839512359348118",
            "extra": "mean: 13.25019982760001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.059907352536929706,
            "unit": "iter/sec",
            "range": "stddev: 0.07634379609296801",
            "extra": "mean: 16.692441873200003 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05260595998329331,
            "unit": "iter/sec",
            "range": "stddev: 0.23417674686454182",
            "extra": "mean: 19.009252949999997 sec\nrounds: 5"
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
          "id": "746a7d69df4e97c5458671649e9464f6d443d09f",
          "message": "Increment version",
          "timestamp": "2026-03-06T18:47:25+01:00",
          "tree_id": "e4a8cadb75c07c4fbd207269f7ec6e8ceb4b7fb5",
          "url": "https://github.com/uroni/hs5/commit/746a7d69df4e97c5458671649e9464f6d443d09f"
        },
        "date": 1772819940849,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07637358433604453,
            "unit": "iter/sec",
            "range": "stddev: 0.1350712331882339",
            "extra": "mean: 13.093532386799996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06119802952545909,
            "unit": "iter/sec",
            "range": "stddev: 0.0786930563954964",
            "extra": "mean: 16.34039539759999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.055149375218130466,
            "unit": "iter/sec",
            "range": "stddev: 0.19272531351451114",
            "extra": "mean: 18.13257169360004 sec\nrounds: 5"
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
          "id": "5d301ab7e8e6617ea8d15611031ec0f7a878bcb7",
          "message": "Fix free space mismatch with WAL after startup",
          "timestamp": "2026-03-07T18:48:21+01:00",
          "tree_id": "a0533a24d2a9a602ecb0572bca9d3ae1f8f7d885",
          "url": "https://github.com/uroni/hs5/commit/5d301ab7e8e6617ea8d15611031ec0f7a878bcb7"
        },
        "date": 1772908390150,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0798247021914144,
            "unit": "iter/sec",
            "range": "stddev: 0.07151995885895754",
            "extra": "mean: 12.527450432599995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06154790568469643,
            "unit": "iter/sec",
            "range": "stddev: 0.1022447322256569",
            "extra": "mean: 16.24750653779995 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05411670431558003,
            "unit": "iter/sec",
            "range": "stddev: 0.3282954477977119",
            "extra": "mean: 18.478582771199967 sec\nrounds: 5"
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
          "id": "5d301ab7e8e6617ea8d15611031ec0f7a878bcb7",
          "message": "Fix free space mismatch with WAL after startup",
          "timestamp": "2026-03-07T18:48:21+01:00",
          "tree_id": "a0533a24d2a9a602ecb0572bca9d3ae1f8f7d885",
          "url": "https://github.com/uroni/hs5/commit/5d301ab7e8e6617ea8d15611031ec0f7a878bcb7"
        },
        "date": 1772909060190,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0819153072184747,
            "unit": "iter/sec",
            "range": "stddev: 0.05707532934198304",
            "extra": "mean: 12.207730569 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06606787941452638,
            "unit": "iter/sec",
            "range": "stddev: 0.08264921170162182",
            "extra": "mean: 15.135948192399974 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05641439659448873,
            "unit": "iter/sec",
            "range": "stddev: 0.2755910674100024",
            "extra": "mean: 17.725971744200002 sec\nrounds: 5"
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
          "id": "3dab1ef21afbd86a7b4c876bcdf5d6a0c8de17d4",
          "message": "Fix sigv2 pre-signed upload",
          "timestamp": "2026-03-07T20:04:41+01:00",
          "tree_id": "aa3d7122f3dd72260bff3d76c333b337691e8be0",
          "url": "https://github.com/uroni/hs5/commit/3dab1ef21afbd86a7b4c876bcdf5d6a0c8de17d4"
        },
        "date": 1772910903776,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09346411063134925,
            "unit": "iter/sec",
            "range": "stddev: 0.012016238006966311",
            "extra": "mean: 10.699294020400009 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0750130055456152,
            "unit": "iter/sec",
            "range": "stddev: 0.07555460339785972",
            "extra": "mean: 13.331021637200001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06682490803285043,
            "unit": "iter/sec",
            "range": "stddev: 0.17733161010268855",
            "extra": "mean: 14.964480003600011 sec\nrounds: 5"
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
          "id": "a5be89a721991b1ffeb454fff5a9bb2f747b7e0e",
          "message": "Add requirements from API client",
          "timestamp": "2026-03-08T13:02:18+01:00",
          "tree_id": "03131743bf08c9361dd40110c282621fd060a954",
          "url": "https://github.com/uroni/hs5/commit/a5be89a721991b1ffeb454fff5a9bb2f747b7e0e"
        },
        "date": 1772971985597,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07724490296838957,
            "unit": "iter/sec",
            "range": "stddev: 0.06619431767668794",
            "extra": "mean: 12.945837997999991 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.061592015482088314,
            "unit": "iter/sec",
            "range": "stddev: 0.16105734749818382",
            "extra": "mean: 16.23587070779997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05589052834280713,
            "unit": "iter/sec",
            "range": "stddev: 0.13206850692936448",
            "extra": "mean: 17.892119284799993 sec\nrounds: 5"
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
          "id": "a5be89a721991b1ffeb454fff5a9bb2f747b7e0e",
          "message": "Add requirements from API client",
          "timestamp": "2026-03-08T13:02:18+01:00",
          "tree_id": "03131743bf08c9361dd40110c282621fd060a954",
          "url": "https://github.com/uroni/hs5/commit/a5be89a721991b1ffeb454fff5a9bb2f747b7e0e"
        },
        "date": 1772978925156,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.077404683841781,
            "unit": "iter/sec",
            "range": "stddev: 0.09351980299954046",
            "extra": "mean: 12.919114843800013 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.061872000405481915,
            "unit": "iter/sec",
            "range": "stddev: 0.191519583707537",
            "extra": "mean: 16.162399687200015 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.055381486306237133,
            "unit": "iter/sec",
            "range": "stddev: 0.25865351551883287",
            "extra": "mean: 18.056575702400004 sec\nrounds: 5"
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
          "id": "d46597965d30060c259a2350c5e58a008e491c18",
          "message": "Apply the simple bucket permissions",
          "timestamp": "2026-03-08T18:32:18+01:00",
          "tree_id": "01176ef77f2171988415257c0717fe9fdde5b0e9",
          "url": "https://github.com/uroni/hs5/commit/d46597965d30060c259a2350c5e58a008e491c18"
        },
        "date": 1772991871703,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07230735615457574,
            "unit": "iter/sec",
            "range": "stddev: 0.09697909617042409",
            "extra": "mean: 13.829851528000006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06168305402285348,
            "unit": "iter/sec",
            "range": "stddev: 0.12071829237924038",
            "extra": "mean: 16.211908048999998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05479479560100534,
            "unit": "iter/sec",
            "range": "stddev: 0.1193311463166362",
            "extra": "mean: 18.249908390600012 sec\nrounds: 5"
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
          "id": "b6e03a561aa81e7d6810ddb064a62b0e7f4b28ef",
          "message": "Increment version",
          "timestamp": "2026-03-08T20:47:39+01:00",
          "tree_id": "0e4f7d2faa25415717cb3d0b9f8b6a9a68ec1a93",
          "url": "https://github.com/uroni/hs5/commit/b6e03a561aa81e7d6810ddb064a62b0e7f4b28ef"
        },
        "date": 1772999908817,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07523518317797288,
            "unit": "iter/sec",
            "range": "stddev: 0.7029433529225299",
            "extra": "mean: 13.291653688600002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.062147118142456916,
            "unit": "iter/sec",
            "range": "stddev: 0.1597409866883159",
            "extra": "mean: 16.090850708599987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.054333871070595015,
            "unit": "iter/sec",
            "range": "stddev: 0.1482244481871958",
            "extra": "mean: 18.404725823800003 sec\nrounds: 5"
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
          "id": "b6e03a561aa81e7d6810ddb064a62b0e7f4b28ef",
          "message": "Increment version",
          "timestamp": "2026-03-08T20:47:39+01:00",
          "tree_id": "0e4f7d2faa25415717cb3d0b9f8b6a9a68ec1a93",
          "url": "https://github.com/uroni/hs5/commit/b6e03a561aa81e7d6810ddb064a62b0e7f4b28ef"
        },
        "date": 1773000287057,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07808859151388194,
            "unit": "iter/sec",
            "range": "stddev: 0.1629858777104581",
            "extra": "mean: 12.805967947599981 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06411784407366272,
            "unit": "iter/sec",
            "range": "stddev: 0.04043682299550464",
            "extra": "mean: 15.596282352399987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05572057378506522,
            "unit": "iter/sec",
            "range": "stddev: 0.14944767244218454",
            "extra": "mean: 17.94669243459998 sec\nrounds: 5"
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
          "id": "627a6e2b15547dac510bd25bd37985c27e71baf2",
          "message": "Check signature when completing multi-part upload\n\nCompleting the multi-part upload needs the upload-id (including the nonce),\nbut the signature should still be checked.\n\nAlso add runtime checks to make sure that the signature is always checked\nwhen the data store is accessed.",
          "timestamp": "2026-03-08T22:07:50+01:00",
          "tree_id": "3562dcd3302c677e459ec42b45a1f08fe8455393",
          "url": "https://github.com/uroni/hs5/commit/627a6e2b15547dac510bd25bd37985c27e71baf2"
        },
        "date": 1773004796229,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07624399534496225,
            "unit": "iter/sec",
            "range": "stddev: 0.07626275788885581",
            "extra": "mean: 13.115786960999992 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06097419012897751,
            "unit": "iter/sec",
            "range": "stddev: 0.1865582441346589",
            "extra": "mean: 16.400381831799972 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.053279599351918464,
            "unit": "iter/sec",
            "range": "stddev: 0.21159141157918093",
            "extra": "mean: 18.768909904800033 sec\nrounds: 5"
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
          "id": "ee3f319e483b14ea6b6bfbbb31697f11b92279e1",
          "message": "Remove CreateBucket from per-bucket write permissions",
          "timestamp": "2026-03-08T22:11:19+01:00",
          "tree_id": "4ef2a9a1b8bf6a52ab403a9bed4ab37de4355a58",
          "url": "https://github.com/uroni/hs5/commit/ee3f319e483b14ea6b6bfbbb31697f11b92279e1"
        },
        "date": 1773004964759,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07651595249695288,
            "unit": "iter/sec",
            "range": "stddev: 0.05181423403571704",
            "extra": "mean: 13.069170119 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06256401859370551,
            "unit": "iter/sec",
            "range": "stddev: 0.18480565447902275",
            "extra": "mean: 15.98362800980001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05534845111055013,
            "unit": "iter/sec",
            "range": "stddev: 0.12009530110557153",
            "extra": "mean: 18.067352923800012 sec\nrounds: 5"
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
          "id": "ee3f319e483b14ea6b6bfbbb31697f11b92279e1",
          "message": "Remove CreateBucket from per-bucket write permissions",
          "timestamp": "2026-03-08T22:11:19+01:00",
          "tree_id": "4ef2a9a1b8bf6a52ab403a9bed4ab37de4355a58",
          "url": "https://github.com/uroni/hs5/commit/ee3f319e483b14ea6b6bfbbb31697f11b92279e1"
        },
        "date": 1773005947019,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07053248347971336,
            "unit": "iter/sec",
            "range": "stddev: 0.18503181755995227",
            "extra": "mean: 14.177864590399986 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.056483865713297124,
            "unit": "iter/sec",
            "range": "stddev: 0.0576557044869263",
            "extra": "mean: 17.704170693200012 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05022597047087875,
            "unit": "iter/sec",
            "range": "stddev: 0.31356976361683736",
            "extra": "mean: 19.910018474999994 sec\nrounds: 5"
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
          "id": "1bca0e56d4e39feaa305ae1e2e4c25c92e4a698f",
          "message": "Refactor api function routing",
          "timestamp": "2026-03-10T15:48:46Z",
          "tree_id": "c38b8ea1f8a7f9401085735bfcaf06faf2a32b26",
          "url": "https://github.com/uroni/hs5/commit/1bca0e56d4e39feaa305ae1e2e4c25c92e4a698f"
        },
        "date": 1773158417470,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07372225853474572,
            "unit": "iter/sec",
            "range": "stddev: 0.09277087715964363",
            "extra": "mean: 13.56442436619999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05984275413801821,
            "unit": "iter/sec",
            "range": "stddev: 0.32375616768399906",
            "extra": "mean: 16.71046084699999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.04869172271029493,
            "unit": "iter/sec",
            "range": "stddev: 0.2550881554021415",
            "extra": "mean: 20.537371535399984 sec\nrounds: 5"
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
          "id": "f19bc95db2892bbe12a3a1e0c04b20d2f4d62836",
          "message": "Revert deprecation change",
          "timestamp": "2026-03-16T22:04:07Z",
          "tree_id": "88b9b9e947093f6bed513bed36f782f3cbc070c9",
          "url": "https://github.com/uroni/hs5/commit/f19bc95db2892bbe12a3a1e0c04b20d2f4d62836"
        },
        "date": 1773699303388,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07487122996848758,
            "unit": "iter/sec",
            "range": "stddev: 0.2034679339339934",
            "extra": "mean: 13.356265155800008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.060801975878560885,
            "unit": "iter/sec",
            "range": "stddev: 0.23512535424757458",
            "extra": "mean: 16.446833931799993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05346293564821632,
            "unit": "iter/sec",
            "range": "stddev: 0.38060465067372945",
            "extra": "mean: 18.70454713859999 sec\nrounds: 5"
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
          "id": "f19bc95db2892bbe12a3a1e0c04b20d2f4d62836",
          "message": "Revert deprecation change",
          "timestamp": "2026-03-16T22:04:07Z",
          "tree_id": "88b9b9e947093f6bed513bed36f782f3cbc070c9",
          "url": "https://github.com/uroni/hs5/commit/f19bc95db2892bbe12a3a1e0c04b20d2f4d62836"
        },
        "date": 1773701219051,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07689478987805586,
            "unit": "iter/sec",
            "range": "stddev: 0.26371438785952445",
            "extra": "mean: 13.004782269199996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06361075267674013,
            "unit": "iter/sec",
            "range": "stddev: 0.16188203206157403",
            "extra": "mean: 15.720612599599997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05436175042743403,
            "unit": "iter/sec",
            "range": "stddev: 0.3519377416168451",
            "extra": "mean: 18.39528698280001 sec\nrounds: 5"
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
          "id": "906113d747da39014bb1bbcf949a7a93e85566a0",
          "message": "Add no permission test",
          "timestamp": "2026-03-16T22:46:10Z",
          "tree_id": "d68c713ab4a0acab3684b2c03d46a7d7090f962e",
          "url": "https://github.com/uroni/hs5/commit/906113d747da39014bb1bbcf949a7a93e85566a0"
        },
        "date": 1773701823953,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07754538716238049,
            "unit": "iter/sec",
            "range": "stddev: 0.14752888789322857",
            "extra": "mean: 12.895673573800002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05924478550854149,
            "unit": "iter/sec",
            "range": "stddev: 0.3256733965558514",
            "extra": "mean: 16.8791226336 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.04933007723991586,
            "unit": "iter/sec",
            "range": "stddev: 1.0835068957113532",
            "extra": "mean: 20.271608234800034 sec\nrounds: 5"
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
          "id": "de326974aa0e1fe39812dbabc670aa728de7a57d",
          "message": "Fix metadata handling in CopyObject",
          "timestamp": "2026-03-16T23:43:49Z",
          "tree_id": "3fff96b8d64fb16d0315c1362152bdfd0c9fafc7",
          "url": "https://github.com/uroni/hs5/commit/de326974aa0e1fe39812dbabc670aa728de7a57d"
        },
        "date": 1773705202800,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.09230854891057626,
            "unit": "iter/sec",
            "range": "stddev: 0.12527100617556577",
            "extra": "mean: 10.833232802400005 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.07394622963285206,
            "unit": "iter/sec",
            "range": "stddev: 0.35890963711472634",
            "extra": "mean: 13.52333993179999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06748163192708331,
            "unit": "iter/sec",
            "range": "stddev: 0.48946344632942446",
            "extra": "mean: 14.818847313599964 sec\nrounds: 5"
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
          "id": "5ec3bb23099182f52edfacc88075c964daeb69f0",
          "message": "Run tests in parallel",
          "timestamp": "2026-03-17T11:32:21Z",
          "tree_id": "8b7d68c373a6660492ec17f01359e94d3d54697d",
          "url": "https://github.com/uroni/hs5/commit/5ec3bb23099182f52edfacc88075c964daeb69f0"
        },
        "date": 1773747852447,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07475010653040962,
            "unit": "iter/sec",
            "range": "stddev: 0.1450107030616508",
            "extra": "mean: 13.377907355799994 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.059035416875530045,
            "unit": "iter/sec",
            "range": "stddev: 0.3177419207529171",
            "extra": "mean: 16.938984306800013 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.054496580937591646,
            "unit": "iter/sec",
            "range": "stddev: 0.35998330552989444",
            "extra": "mean: 18.349775028 sec\nrounds: 5"
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
          "id": "5ec3bb23099182f52edfacc88075c964daeb69f0",
          "message": "Run tests in parallel",
          "timestamp": "2026-03-17T11:32:21Z",
          "tree_id": "8b7d68c373a6660492ec17f01359e94d3d54697d",
          "url": "https://github.com/uroni/hs5/commit/5ec3bb23099182f52edfacc88075c964daeb69f0"
        },
        "date": 1773749730249,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07648615743734005,
            "unit": "iter/sec",
            "range": "stddev: 0.07447953482493201",
            "extra": "mean: 13.074261193199993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.061148286950222404,
            "unit": "iter/sec",
            "range": "stddev: 0.1927870017275895",
            "extra": "mean: 16.35368789339998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05406512197299593,
            "unit": "iter/sec",
            "range": "stddev: 0.23168029137667934",
            "extra": "mean: 18.49621278020002 sec\nrounds: 5"
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
          "id": "82c80aa983b9fe73eec86157cb607d87df4ae649",
          "message": "Ignore case of x-amz-meta prefix",
          "timestamp": "2026-03-18T21:02:43Z",
          "tree_id": "7a2dd60ca4d296867326ff942a81512599dc4ddd",
          "url": "https://github.com/uroni/hs5/commit/82c80aa983b9fe73eec86157cb607d87df4ae649"
        },
        "date": 1773868444294,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07519901251419284,
            "unit": "iter/sec",
            "range": "stddev: 0.14416659429718073",
            "extra": "mean: 13.298046963199988 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.061444589084627556,
            "unit": "iter/sec",
            "range": "stddev: 0.24007295075523863",
            "extra": "mean: 16.2748260652 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05354061876405219,
            "unit": "iter/sec",
            "range": "stddev: 0.1597923908119327",
            "extra": "mean: 18.67740835060001 sec\nrounds: 5"
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
          "id": "82c80aa983b9fe73eec86157cb607d87df4ae649",
          "message": "Ignore case of x-amz-meta prefix",
          "timestamp": "2026-03-18T21:02:43Z",
          "tree_id": "7a2dd60ca4d296867326ff942a81512599dc4ddd",
          "url": "https://github.com/uroni/hs5/commit/82c80aa983b9fe73eec86157cb607d87df4ae649"
        },
        "date": 1773869034504,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0784697586415788,
            "unit": "iter/sec",
            "range": "stddev: 0.14366764989859487",
            "extra": "mean: 12.743762913399985 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06416055491435727,
            "unit": "iter/sec",
            "range": "stddev: 0.12228673216984312",
            "extra": "mean: 15.585900111600017 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.056684082198563926,
            "unit": "iter/sec",
            "range": "stddev: 0.21180683530152955",
            "extra": "mean: 17.641636967800014 sec\nrounds: 5"
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
          "id": "286609fb1002578f69b2e34e1ddfef2385fcc7de",
          "message": "Add Hetzner storage box use case",
          "timestamp": "2026-03-18T21:51:44Z",
          "tree_id": "f8c095ee3026688f25e312a59ec252bd0134f060",
          "url": "https://github.com/uroni/hs5/commit/286609fb1002578f69b2e34e1ddfef2385fcc7de"
        },
        "date": 1773871365875,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07662589151339813,
            "unit": "iter/sec",
            "range": "stddev: 0.1737262008882762",
            "extra": "mean: 13.050419123999996 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.061680627575686646,
            "unit": "iter/sec",
            "range": "stddev: 0.22765707622759135",
            "extra": "mean: 16.212545807400012 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05431600052856468,
            "unit": "iter/sec",
            "range": "stddev: 0.19211041144045746",
            "extra": "mean: 18.410781174400018 sec\nrounds: 5"
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
          "id": "798aea73b96f7557bfb55a91a43f2830c4956e9a",
          "message": "Fix page order",
          "timestamp": "2026-03-18T21:52:45Z",
          "tree_id": "3ad9a75d6237d8f4b47778850ef2ce2c44c8654e",
          "url": "https://github.com/uroni/hs5/commit/798aea73b96f7557bfb55a91a43f2830c4956e9a"
        },
        "date": 1773871387023,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0779502257356331,
            "unit": "iter/sec",
            "range": "stddev: 0.1673540338773198",
            "extra": "mean: 12.8286992188 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06164916664130804,
            "unit": "iter/sec",
            "range": "stddev: 0.2631942740916573",
            "extra": "mean: 16.220819428400024 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.055981866379728365,
            "unit": "iter/sec",
            "range": "stddev: 0.17113588315220302",
            "extra": "mean: 17.86292713459998 sec\nrounds: 5"
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
          "id": "145509a17ae6a696d52d6b3f71e470cfed914591",
          "message": "Replace VM with VPS",
          "timestamp": "2026-03-18T21:57:28Z",
          "tree_id": "b429093fea152e25efa61a23cde2a54d27946a36",
          "url": "https://github.com/uroni/hs5/commit/145509a17ae6a696d52d6b3f71e470cfed914591"
        },
        "date": 1773871707188,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07706811222157425,
            "unit": "iter/sec",
            "range": "stddev: 0.10024609757088992",
            "extra": "mean: 12.975535162000018 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06240809090243693,
            "unit": "iter/sec",
            "range": "stddev: 0.13962923645773215",
            "extra": "mean: 16.023563380000006 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.053459414964553746,
            "unit": "iter/sec",
            "range": "stddev: 0.21517440250826644",
            "extra": "mean: 18.705778966400022 sec\nrounds: 5"
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
          "id": "d2ff0469579e27c3fe8a59f4d6adf5b05c31f708",
          "message": "Perhaps improve formatting",
          "timestamp": "2026-03-18T21:59:18Z",
          "tree_id": "2f7bdd559cf18dfb67a6c517a66a11039bb71edc",
          "url": "https://github.com/uroni/hs5/commit/d2ff0469579e27c3fe8a59f4d6adf5b05c31f708"
        },
        "date": 1773871815428,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0745305952748411,
            "unit": "iter/sec",
            "range": "stddev: 0.05808848635694217",
            "extra": "mean: 13.41730864100001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0610810756993695,
            "unit": "iter/sec",
            "range": "stddev: 0.16339465086419255",
            "extra": "mean: 16.371682858399993 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05308483989717166,
            "unit": "iter/sec",
            "range": "stddev: 0.26181845928319014",
            "extra": "mean: 18.837769915800003 sec\nrounds: 5"
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
          "id": "881226ff9e906764f619514c38da3efb3a491f90",
          "message": "Indent in markdown list",
          "timestamp": "2026-03-18T22:03:14Z",
          "tree_id": "09b40d2d8afcf695a9b6197d56731e8f4fe435ec",
          "url": "https://github.com/uroni/hs5/commit/881226ff9e906764f619514c38da3efb3a491f90"
        },
        "date": 1773872022957,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.0816464348794569,
            "unit": "iter/sec",
            "range": "stddev: 0.21544168778978764",
            "extra": "mean: 12.247932215000002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06563944992885458,
            "unit": "iter/sec",
            "range": "stddev: 0.2426389547057256",
            "extra": "mean: 15.234740709800008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.0559304388475062,
            "unit": "iter/sec",
            "range": "stddev: 0.3086486017802308",
            "extra": "mean: 17.879351934399985 sec\nrounds: 5"
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
          "id": "3ce3ffa76043c92b872e17453126688d50dcdbe5",
          "message": "Add session expiration handling and improve logout process",
          "timestamp": "2026-03-31T11:33:18Z",
          "tree_id": "17220d3496ac12573b3e968612bf47d11f01080d",
          "url": "https://github.com/uroni/hs5/commit/3ce3ffa76043c92b872e17453126688d50dcdbe5"
        },
        "date": 1774957465085,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06977310418412468,
            "unit": "iter/sec",
            "range": "stddev: 0.4940151669036862",
            "extra": "mean: 14.332170134799991 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05954567214691779,
            "unit": "iter/sec",
            "range": "stddev: 0.12732889006747813",
            "extra": "mean: 16.79383175879999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05202368402583166,
            "unit": "iter/sec",
            "range": "stddev: 0.7103917333205",
            "extra": "mean: 19.222014333000015 sec\nrounds: 5"
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
          "id": "0eadcf7c9e7ce5f25082b637e2ec2866b231dd28",
          "message": "Allow wildcards in action in policy document",
          "timestamp": "2026-03-31T12:19:30Z",
          "tree_id": "af224ba45f9195b8359690620797b3d21dd9ba99",
          "url": "https://github.com/uroni/hs5/commit/0eadcf7c9e7ce5f25082b637e2ec2866b231dd28"
        },
        "date": 1774960282443,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07053804484223467,
            "unit": "iter/sec",
            "range": "stddev: 0.11889422112980119",
            "extra": "mean: 14.176746778799998 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.0568549565079157,
            "unit": "iter/sec",
            "range": "stddev: 0.3372018646629279",
            "extra": "mean: 17.58861604020001 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05048016916365476,
            "unit": "iter/sec",
            "range": "stddev: 0.17178307787740546",
            "extra": "mean: 19.809759288999977 sec\nrounds: 5"
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
          "id": "0eadcf7c9e7ce5f25082b637e2ec2866b231dd28",
          "message": "Allow wildcards in action in policy document",
          "timestamp": "2026-03-31T12:19:30Z",
          "tree_id": "af224ba45f9195b8359690620797b3d21dd9ba99",
          "url": "https://github.com/uroni/hs5/commit/0eadcf7c9e7ce5f25082b637e2ec2866b231dd28"
        },
        "date": 1774984757926,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07403224287362521,
            "unit": "iter/sec",
            "range": "stddev: 0.05430354996363759",
            "extra": "mean: 13.507628044000011 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.059867494327222895,
            "unit": "iter/sec",
            "range": "stddev: 0.0827384862158481",
            "extra": "mean: 16.70355526379999 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.053806721465909896,
            "unit": "iter/sec",
            "range": "stddev: 0.3132853784389855",
            "extra": "mean: 18.585038685799987 sec\nrounds: 5"
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
          "id": "dbd25ea7e3543c541ed97e08faed6abd6bc26fba",
          "message": "Add psutil to dependencies",
          "timestamp": "2026-03-31T21:48:44Z",
          "tree_id": "36a54c3a1b620a15fe797242b05dff6fefdc56c7",
          "url": "https://github.com/uroni/hs5/commit/dbd25ea7e3543c541ed97e08faed6abd6bc26fba"
        },
        "date": 1774994377172,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07396921388420868,
            "unit": "iter/sec",
            "range": "stddev: 0.06496730713656386",
            "extra": "mean: 13.519137861399997 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05985105250580184,
            "unit": "iter/sec",
            "range": "stddev: 0.10892542822223958",
            "extra": "mean: 16.708143936199985 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.04974632197107652,
            "unit": "iter/sec",
            "range": "stddev: 0.44385202428499704",
            "extra": "mean: 20.101988657200014 sec\nrounds: 5"
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
          "id": "c7b7ef9282f10426442cffcf1cf7ab7af6644b4a",
          "message": "Add HS5 prefix to env variables and make more configurable via env variables",
          "timestamp": "2026-04-02T20:13:24Z",
          "tree_id": "813c73792990c8d5dc552c22736b0f9b85a3c8af",
          "url": "https://github.com/uroni/hs5/commit/c7b7ef9282f10426442cffcf1cf7ab7af6644b4a"
        },
        "date": 1775161498309,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07582431842965447,
            "unit": "iter/sec",
            "range": "stddev: 0.21543831045833878",
            "extra": "mean: 13.188380993200008 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06146482557521633,
            "unit": "iter/sec",
            "range": "stddev: 0.1456241654742069",
            "extra": "mean: 16.269467791399983 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05368550713521426,
            "unit": "iter/sec",
            "range": "stddev: 0.7312833482055163",
            "extra": "mean: 18.62700109140003 sec\nrounds: 5"
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
          "id": "65dad79cdfe659c4a080cf9df21e2e10d2704d8f",
          "message": "Add env prefix to documentation",
          "timestamp": "2026-04-02T21:30:31Z",
          "tree_id": "15c8b317e6c57d385a08c9b59b0515a2dd23d42a",
          "url": "https://github.com/uroni/hs5/commit/65dad79cdfe659c4a080cf9df21e2e10d2704d8f"
        },
        "date": 1775166115749,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.06807373644912654,
            "unit": "iter/sec",
            "range": "stddev: 0.5235970326501012",
            "extra": "mean: 14.689953161999984 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.05726043496942089,
            "unit": "iter/sec",
            "range": "stddev: 0.17974154979166057",
            "extra": "mean: 17.464065729399987 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.05063073493271973,
            "unit": "iter/sec",
            "range": "stddev: 0.3414532302383329",
            "extra": "mean: 19.75084899179999 sec\nrounds: 5"
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
          "id": "65dad79cdfe659c4a080cf9df21e2e10d2704d8f",
          "message": "Add env prefix to documentation",
          "timestamp": "2026-04-02T21:30:31Z",
          "tree_id": "15c8b317e6c57d385a08c9b59b0515a2dd23d42a",
          "url": "https://github.com/uroni/hs5/commit/65dad79cdfe659c4a080cf9df21e2e10d2704d8f"
        },
        "date": 1775227164641,
        "tool": "pytest",
        "benches": [
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_hs5",
            "value": 0.07529780763621326,
            "unit": "iter/sec",
            "range": "stddev: 0.1081240443263454",
            "extra": "mean: 13.280599148800002 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_minio",
            "value": 0.06612039483457928,
            "unit": "iter/sec",
            "range": "stddev: 0.3344744942655561",
            "extra": "mean: 15.123926626600019 sec\nrounds: 5"
          },
          {
            "name": "test/test_perf.py::test_perf_upload_many_files_garage_sqlite",
            "value": 0.06088198961476361,
            "unit": "iter/sec",
            "range": "stddev: 0.18734993878260056",
            "extra": "mean: 16.425218793400017 sec\nrounds: 5"
          }
        ]
      }
    ]
  }
}