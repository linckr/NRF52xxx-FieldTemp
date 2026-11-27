#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ADC 校准记录工具：同时读取 OpenDM40 万用表与 PandaTemp 的 BLE 实时电压。

用法：
  # 采数（手机端请先断开 App，避免占用唯一 BLE 连接）
  python tools/adc_calibrate.py run --interval 5 --out exports/adc_cal.csv
  # 也可先扫广播确认设备可见
  python tools/adc_calibrate.py scan
  # 分析已有 CSV（输出各电压点误差与线性拟合）
  python tools/adc_calibrate.py analyze exports/adc_cal.csv

说明：
- 万用表：Alientek DM40A (opendm40)，USBTMC/SCPI，读数命令 MEAS:VOLT:DC?。
- 温湿度计实时特征 12340031-1234-5678-1234-56789abcdef0，offset 6 为 uint16 LE 电池 mV。
- 采样前设备应仅电池供电、断开 SWD/USB；每改变一次电压等 >=60 s 再采。
"""
import argparse
import asyncio
import csv
import datetime as dt
import os
import sys

METER_VID = 0x1209
METER_PID = 0xD040
UUID_REALTIME = "12340031-1234-5678-1234-56789abcdef0"
INVALID_MV = 0xFFFF


def _setup_libusb():
    try:
        import libusb_package
        d = os.path.dirname(libusb_package.__file__)
        os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")
        if hasattr(os, "add_dll_directory"):
            try:
                os.add_dll_directory(d)
            except Exception:
                pass
    except Exception:
        pass


def meter_serial():
    import usb.core
    import libusb_package
    _setup_libusb()
    backend = libusb_package.get_libusb1_backend()
    for dev in usb.core.find(find_all=True, backend=backend):
        if dev.idVendor == METER_VID and dev.idProduct == METER_PID:
            return dev.serial_number
    return None


class Meter:
    def __init__(self, serial=None):
        _setup_libusb()
        import pyvisa
        self.rm = pyvisa.ResourceManager("@py")
        sn = serial or meter_serial()
        if not sn:
            raise RuntimeError("未找到 OpenDM40 万用表（VID 0x1209/PID 0xD040）")
        res = "USB0::0x%04X::0x%04X::%s::INSTR" % (METER_VID, METER_PID, sn)
        self.dev = self.rm.open_resource(
            res, read_termination="\n", write_termination="\n",
            timeout=5000, encoding="utf-8")

    def read_v(self):
        try:
            s = self.dev.query("MEAS:VOLT:DC?").strip()
        except Exception:
            s = self.dev.query("READ?").strip()
        return float(s)

    def close(self):
        try:
            self.dev.close()
        except Exception:
            pass


async def find_pandatemp(address=None):
    from bleak import BleakScanner
    if address:
        return address
    devs = await BleakScanner.discover(timeout=12.0, return_adv=True)
    items = devs.values() if isinstance(devs, dict) else devs
    for it in items:
        dev = it[0] if isinstance(it, (tuple, list)) else it
        name = getattr(dev, "name", None) or ""
        if name.startswith("PandaTemp"):
            print("找到 BLE: %s %s" % (name, dev.address))
            return dev
    raise RuntimeError("未扫描到 PandaTemp；请关闭手机 App/断开连接后重试，或使用 --address")


async def read_app_mv(client):
    raw = bytes(await client.read_gatt_char(UUID_REALTIME))
    if len(raw) < 8:
        raise RuntimeError("实时帧长度 %d < 8（固件未启用 ADC？）" % len(raw))
    mv = int.from_bytes(raw[6:8], "little")
    return None if mv == INVALID_MV else mv


def analyze(path, min_points=3):
    rows = []
    with open(path, encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            try:
                rows.append((float(r["meter_V"]), float(r["app_mv"])))
            except Exception:
                pass
    if not rows:
        print("没有有效样本")
        return
    rows.sort()
    mv = [r[1] for r in rows]
    ref = [r[0] * 1000.0 for r in rows]
    err = [a - b for a, b in zip(mv, ref)]
    mean_err = sum(err) / len(err)
    mae = sum(abs(e) for e in err) / len(err)
    print("样本 %d；误差(App-表) 均值 %.2f mV，MAE %.2f mV，最大 %.2f mV"
          % (len(rows), mean_err, mae, max(abs(e) for e in err)))
    # group by 10 mV of meter reading
    groups = {}
    for m, a in rows:
        k = round(m * 1000.0 / 10.0) * 10
        groups.setdefault(k, []).append(a)
    print("表压(mV)  n   App均值(mV)  误差(mV)")
    for k in sorted(groups):
        vals = groups[k]
        if len(vals) < min_points:
            continue
        am = sum(vals) / len(vals)
        print("%8d  %2d   %10.1f   %+7.1f" % (k, len(vals), am, am - k))
    # least squares: ref = a*mv + b
    n = len(rows)
    sx = sum(mv); sy = sum(ref); sxx = sum(x * x for x in mv); sxy = sum(x * y for x, y in zip(mv, ref))
    den = n * sxx - sx * sx
    if den != 0:
        a = (n * sxy - sx * sy) / den
        b = (sy - a * sx) / n
        print("拟合: 表压_mV = %.6f × App_mV + %.2f" % (a, b))
        print("建议校正: App_true = App×%.6f + %.2f mV" % (a, b))
    if mae <= 20.0:
        print("结论: MAE<=20 mV，可认为当前精度足够，暂不需要校正。")
    else:
        print("结论: 误差超过 20 mV，建议加入校正系数后复测。")


async def run(args):
    meter = Meter(args.meter_serial)
    print("万用表已连接: %.6f V" % meter.read_v())
    address = await find_pandatemp(args.address)
    from bleak import BleakClient
    client = None
    out = args.out
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    started = dt.datetime.now().isoformat(timespec="seconds")
    with open(out, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["time", "meter_V", "app_mv", "app_V", "err_mv"])
        print("记录到 %s（Ctrl+C 结束；换电池时设备会重启，脚本会自动重连）" % out)
        count = 0
        deadline = None if args.duration <= 0 else dt.datetime.now() + dt.timedelta(seconds=args.duration)
        try:
            while deadline is None or dt.datetime.now() < deadline:
                t = dt.datetime.now().isoformat(timespec="seconds")
                if client is None or not client.is_connected:
                    try:
                        client = BleakClient(address, timeout=10.0, winrt={"use_cached_services": False})
                        await asyncio.wait_for(client.connect(), timeout=10.0)
                        print("%s  BLE 已连接 %s" % (t, address))
                    except Exception as e:
                        client = None
                mv = None
                if client is not None:
                    try:
                        mv = await read_app_mv(client)
                    except Exception:
                        try:
                            await client.disconnect()
                        except Exception:
                            pass
                        client = None
                v = await asyncio.to_thread(meter.read_v)
                if mv is None:
                    w.writerow([t, "%.6f" % v, "", "", ""])
                    f.flush()
                    print("%s  表 %.6f V  App=--（等待 BLE）" % (t, v))
                else:
                    err = mv - v * 1000.0
                    w.writerow([t, "%.6f" % v, mv, "%.3f" % (mv / 1000.0), "%.1f" % err])
                    f.flush()
                    print("%s  表 %.6f V  App %d mV  误差 %+.1f mV" % (t, v, mv, err))
                count += 1
                await asyncio.sleep(args.interval)
        except KeyboardInterrupt:
            print("\n已停止")
    if client is not None:
        try:
            await client.disconnect()
        except Exception:
            pass
    meter.close()
    print("开始时间 %s，共 %d 次，文件 %s" % (started, count, out))
    if count:
        print("\n=== 汇总 ===")
        analyze(out, args.min_points)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p_scan = sub.add_parser("scan", help="扫描 PandaTemp 广播")
    p_run = sub.add_parser("run", help="同时采集万用表与 BLE 电压")
    p_run.add_argument("--interval", type=float, default=5.0)
    p_run.add_argument("--duration", type=float, default=0)
    p_run.add_argument("--out", default="exports/adc_cal_%s.csv" % dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    p_run.add_argument("--address", default=None)
    p_run.add_argument("--meter-serial", default=None)
    p_run.add_argument("--min-points", type=int, default=3)
    p_an = sub.add_parser("analyze", help="分析已有 CSV")
    p_an.add_argument("file")
    p_an.add_argument("--min-points", type=int, default=3)
    args = ap.parse_args()

    if args.cmd == "scan":
        async def _s():
            from bleak import BleakScanner
            devs = await BleakScanner.discover(timeout=10.0, return_adv=True)
            items = devs.values() if isinstance(devs, dict) else devs
            for it in items:
                dev = it[0] if isinstance(it, (tuple, list)) else it
                name = getattr(dev, "name", None) or ""
                if name.startswith("PandaTemp"):
                    adv = it[1] if isinstance(it, (tuple, list)) and len(it) > 1 else None
                    print("PandaTemp %s rssi=%s" % (dev.address, getattr(adv, "rssi", "?")))
        asyncio.run(_s())
    elif args.cmd == "run":
        asyncio.run(run(args))
    else:
        analyze(args.file, args.min_points)


if __name__ == "__main__":
    sys.exit(main())
