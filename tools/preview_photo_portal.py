#!/usr/bin/env python3
"""Local UI smoke-test server; simulates a device, never writes to a real SD.

python3 tools/preview_photo_portal.py --model 10 --port 8765
python3 tools/preview_photo_portal.py --model color --port 8766
"""

import argparse
from datetime import datetime
from email.parser import BytesParser
from email.policy import default
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import mimetypes
import os
import tempfile
import time
from urllib.parse import parse_qs

WEB = Path(__file__).resolve().parents[1] / "FramEink.Core/web"


def jpeg_dimensions(data):
    if data[:2] != b"\xff\xd8" or data[-2:] != b"\xff\xd9":
        raise ValueError("Invalid JPEG signature")
    position = 2
    while position < len(data):
        if data[position] != 255:
            raise ValueError("Invalid JPEG marker")
        while data[position] == 255:
            position += 1
        marker = data[position]
        position += 1
        length = int.from_bytes(data[position:position + 2], "big")
        if marker == 0xc0:
            return (int.from_bytes(data[position + 5:position + 7], "big"),
                    int.from_bytes(data[position + 3:position + 5], "big"))
        if length < 2 or marker in (0xda, 0xd9):
            break
        position += length
    raise ValueError("No baseline frame")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", choices=["10", "color"], default="10")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--force-heic", action="store_true", help="Exercise the offline decoder even in Safari with native HEIC support")
    parser.add_argument("--seed-photo", type=Path, help="Copy a JPEG fixture into the disposable test gallery")
    parser.add_argument("--seed-count", type=int, default=23)
    parser.add_argument("--save-delay", type=float, default=0, help="Simulate a slow SD save, in seconds, for progress UI checks")
    args = parser.parse_args()
    portrait = args.model == "10"
    device = {"name": "FramEInk 10" if portrait else "FramEInk 6 Color",
              "width": 825 if portrait else 600, "height": 1200 if portrait else 448,
              "grayscale": portrait, "directory": "/portrait" if portrait else "/landscape",
              "sdReady": True, "maxPhotoBytes": 2 * 1024 * 1024}
    settings = {"ssid": "Test WiFi", "password": "", "latitude": "48.1",
                "longitude": "11.5", "calendarUrl": "", "nightPauseStart": "23:00", "nightPauseEnd": "05:00", "updateMinute": "00", "photoOrder": "random"}
    output = Path(tempfile.mkdtemp(prefix="frameink-portal-preview-"))
    if args.seed_photo:
        for index in range(args.seed_count):
            (output / f"Sample photo {index + 1:02}.jpg").write_bytes(args.seed_photo.read_bytes())

    class Handler(BaseHTTPRequestHandler):
        def selected_photo(self, identifier):
            try:
                name = bytes.fromhex(identifier).decode("utf-8")
            except (ValueError, UnicodeError):
                return None
            if not name or name.startswith(".") or any(c in name for c in ("/", "\\", "\0", ":")):
                return None
            path = output / name
            return path if path.is_file() and path.suffix.lower() in (".jpg", ".jpeg", ".png", ".bmp") else None

        def reply(self, code, body=b"", content_type="text/plain; charset=utf-8", compressed=False):
            if isinstance(body, str):
                body = body.encode()
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            if compressed:
                self.send_header("Content-Encoding", "gzip")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            route = self.path.split("?", 1)[0]
            if route in ("/", "/foto"):
                self.reply(200, (WEB / "index.html").read_bytes(), "text/html; charset=utf-8")
            elif route == "/portal.js":
                source = (WEB / "portal.js").read_text()
                if args.force_heic:
                    source = source.replace("return await loadImage(file);", "if (await isHeic(file)) throw new Error('Native HEIC disabled by test server'); return await loadImage(file);", 1)
                self.reply(200, source, "application/javascript")
            elif route == "/heic-to.js":
                self.reply(200, (WEB / "vendor/heic-to-1.5.2.js.gz").read_bytes(), "application/javascript", True)
            elif route == "/api/device":
                self.reply(200, json.dumps(device), "application/json")
            elif route == "/api/settings":
                self.reply(200, json.dumps(settings), "application/json")
            elif route == "/api/photos":
                params = parse_qs(self.path.partition("?")[2])
                offset = int(params.get("offset", ["0"])[0])
                sort, order = params.get("sort", ["modified"])[0], params.get("order", ["desc"])[0]
                if sort not in ("name", "modified") or order not in ("asc", "desc"):
                    self.reply(400, "Invalid gallery sort order.")
                    return
                def modified(path):
                    stamp = datetime.fromtimestamp(path.stat().st_mtime)
                    packed = ((stamp.year - 1980) << 25) | (stamp.month << 21) | (stamp.day << 16) | (stamp.hour << 11) | (stamp.minute << 5) | (stamp.second // 2)
                    return packed, stamp.replace(second=stamp.second // 2 * 2, microsecond=0).isoformat()
                def key(path):
                    return ((modified(path)[0],) if sort == "modified" else ()) + (path.name.lower(), path.name)
                photos = sorted((p for p in output.iterdir() if p.is_file() and not p.name.startswith(".") and p.suffix.lower() in (".jpg", ".jpeg", ".png", ".bmp")), key=key, reverse=order == "desc")
                available = photos[offset:]
                if params.get("cursor"):
                    cursor = params["cursor"][0]
                    packed, name = int(cursor[:8], 16), bytes.fromhex(cursor[8:]).decode()
                    cursor_key = ((packed,) if sort == "modified" else ()) + (name.lower(), name)
                    available = [p for p in photos if (key(p) > cursor_key if order == "asc" else key(p) < cursor_key)]
                page = available[:10]
                more = len(available) > len(page)
                self.reply(200, json.dumps({"photos": [{"id": p.name.encode().hex(), "name": p.name, "size": p.stat().st_size, "modified": modified(p)[1]} for p in page], "nextOffset": offset + len(page) if more else None, "nextCursor": f"{modified(page[-1])[0]:08x}" + page[-1].name.encode().hex() if more else None, "pageSize": 10, "totalPhotos": len(photos), "totalPages": (len(photos) + 9) // 10}), "application/json")
            elif route == "/api/photo":
                params = parse_qs(self.path.partition("?")[2])
                photo = self.selected_photo(params.get("id", [""])[0])
                self.reply(200, photo.read_bytes(), mimetypes.guess_type(photo.name)[0]) if photo else self.reply(404, "Photo not found. Refresh the gallery.")
            else:
                self.reply(404)

        def do_POST(self):
            data = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            if self.path == "/api/ping":
                self.reply(204)
            elif self.path == "/api/settings":
                for key, value in parse_qs(data.decode(), keep_blank_values=True).items():
                    settings[key] = value[0]
                self.reply(200, "Settings saved. You can upload photos or tap Finish.")
            elif self.path == "/api/finish":
                self.reply(200, "All done. FramEInk will display the calendar and go to sleep.")
            elif self.path == "/api/photos/delete":
                if self.headers.get("X-FramEInk-Request") != "1":
                    self.reply(403, "Delete photos from the FramEInk settings page.")
                    return
                params = parse_qs(data.decode())
                photo = self.selected_photo(params.get("id", [""])[0])
                if photo:
                    photo.unlink() # Only disposable fixtures under output, never a real SD.
                    self.reply(200, "Photo deleted.")
                else:
                    self.reply(404, "Photo not found. Refresh the gallery.")
            elif self.path == "/api/photos":
                message = BytesParser(policy=default).parsebytes(
                    b"Content-Type: " + self.headers["Content-Type"].encode() + b"\r\n\r\n" + data)
                parts = list(message.iter_parts())
                if len(parts) != 1 or parts[0].get_content_type() != "image/jpeg":
                    self.reply(415, "Expected one JPEG, never the original HEIC")
                    return
                jpeg = parts[0].get_payload(decode=True)
                try:
                    dimensions = jpeg_dimensions(jpeg)
                    if dimensions != (device["width"], device["height"]):
                        raise ValueError(f"Wrong target dimensions: {dimensions}")
                except ValueError as error:
                    self.reply(422, str(error))
                    return
                stamp = self.headers.get("X-FramEInk-Upload-Time", "")
                uploaded = None
                if stamp:
                    try:
                        if len(stamp) != 17 or not stamp.isascii() or not stamp.isdigit():
                            raise ValueError("Invalid photo upload time.")
                        uploaded = datetime.strptime(stamp, "%Y%m%d%H%M%S%f")
                        if not 1980 <= uploaded.year <= 2099:
                            raise ValueError("Invalid photo upload year.")
                    except ValueError as error:
                        self.reply(400, str(error))
                        return
                prefix = f"photo_{stamp}_" if stamp else "photo_"
                index = 1
                while (output / f"{prefix}{index:03}.jpg").exists():
                    index += 1
                path = output / f"{prefix}{index:03}.jpg"
                if args.save_delay > 0:
                    time.sleep(args.save_delay)
                path.write_bytes(jpeg)
                if uploaded:
                    os.utime(path, (uploaded.timestamp(), uploaded.timestamp()))
                print(f"JPEG verified: {dimensions}, {len(jpeg)} bytes -> {path}", flush=True)
                self.reply(201, json.dumps({"path": device["directory"] + "/" + path.name}), "application/json")
            else:
                self.reply(404)

    print(f"Preview {device['name']}: http://127.0.0.1:{args.port} (JPEGs in {output})", flush=True)
    HTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
