import urllib.request
import urllib.parse

models = [
    "TB331FC", "TB320FC", "TB330FU", "TB330XU", "TB371FC", "TB372FC",
    "TB373FU", "TB375FC", "TB351FU", "TB352FU", "TB132FU", "TB138FC",
    "TB520FU", "TB570FU", "TB310FU", "TB328FU"
]
regions = ["PRC", "CN", "ROW", "UNKNOWN"]
serials = ["HMFA19LS", "HA123456"]

for m in models:
    for r in regions:
        url = (
            f"https://ota.lenovo.com/engine/upgrade?"
            f"devicemodel={m}_OLYMPIA_{r}&"
            f"deviceid=HMFA19LS&"
            f"curfirmwarever={m}_OLYMPIA_S000002_{r}&"
            f"action=querynewfirmware&locale=en"
        )
        req = urllib.request.Request(
            url,
            data=b"",
            headers={"Content-Type": "application/json;charset=UTF-8"},
            method="POST"
        )
        try:
            with urllib.request.urlopen(req, timeout=5) as resp:
                body = resp.read().decode("utf-8", "replace")
                if "downloadurl" in body.lower() or "http" in body.lower() or "succ" in body.lower():
                    print(f"=== HIT: {m} {r} ===")
                    print(body)
                else:
                    # print short summary
                    print(f"[{m}_{r}]: {body.strip()[:120]}")
        except Exception as e:
            print(f"[{m}_{r}] ERR: {e}")
