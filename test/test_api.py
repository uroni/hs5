import requests
from hs5_fixture import Hs5Runner, hs5

def test_login(hs5: Hs5Runner):
    url = hs5.get_api_url()

    ses = requests.session()

    response = ses.post(url+"login", json={
        "username": "root",
        "password": hs5.get_root_key()
    })

    assert response.status_code == 200
    resp = response.json()
    assert resp["ses"]

def test_adduser(hs5: Hs5Runner):
    url = hs5.get_api_url()

    ses1 = requests.session()

    response = ses1.post(url+"adduser", json={
        "ses": "foobar",
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 401

    response = ses1.post(url+"login", json={
        "username": "root",
        "password": hs5.get_root_key()
    })

    assert response.status_code == 200
    resp = response.json()
    sesVal = resp["ses"]

    response = ses1.post(url+"adduser", json={
        "ses": sesVal,
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 200

    ses2 = requests.session()

    response = ses2.post(url+"login", json={
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 200