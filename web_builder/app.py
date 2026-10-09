import os
import socket
import subprocess
import logging
from flask import Flask, request, render_template, send_file, redirect, url_for

# Disable default Flask/Werkzeug logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

app = Flask(__name__)

# Config
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__name__), ".."))
ROM_NAME = "Pokemon FireRed (U) 1.0 (Squirrels).gba"
ROM_PATH = os.path.join(PROJECT_ROOT, ROM_NAME)
CIA_PATH = os.path.join(PROJECT_ROOT, "3ds_port", "dist", "Pokemon FireLeaf.cia")

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

@app.route("/", methods=["GET"])
def index():
    return render_template("index.html")

@app.route("/build", methods=["POST"])
def build():
    if "rom_file" not in request.files:
        return "Missing GBA file", 400
    
    file = request.files["rom_file"]
    if file.filename == "":
        return "No file selected", 400

    print(f"\n[+] Received file: {file.filename}")
    print("[+] Saving ROM and starting compilation process. This may take a few minutes...")
    
    file.save(ROM_PATH)

    try:
        process = subprocess.run(
            ["make", "release"],
            cwd=os.path.join(PROJECT_ROOT, "3ds_port"),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        if process.returncode != 0:
            print("[-] Compilation Error!")
            return f"<h1>Compilation Error!</h1><pre>{process.stdout}</pre>", 500
    except Exception as e:
        print(f"[-] System Error: {str(e)}")
        return f"<h1>System Error!</h1><p>{str(e)}</p>", 500

    print("[+] Compilation finished successfully! Ready for download.")
    return redirect(url_for("success"))

@app.route("/success", methods=["GET"])
def success():
    if not os.path.exists(CIA_PATH):
        return "<h1>.cia file not found. Compilation might have failed.</h1>", 404

    local_ip = get_local_ip()
    download_url = f"http://{local_ip}:5000/download/cia"
    qr_url = f"https://api.qrserver.com/v1/create-qr-code/?size=300x300&data={download_url}"

    return render_template("success.html", download_url=download_url, qr_url=qr_url)

@app.route("/download/cia", methods=["GET"])
def download_cia():
    if os.path.exists(CIA_PATH):
        client_ip = request.remote_addr
        print(f"\n[+] Sending file: Pokemon FireLeaf.cia to {client_ip}...")
        return send_file(CIA_PATH, as_attachment=True)
    return "File not found", 404

if __name__ == "__main__":
    ip = get_local_ip()
    print("="*50)
    print("🔥 FIRELEAF 3DS WEB BUILDER SERVER 🔥")
    print("="*50)
    print(f"[+] Website running on: http://{ip}:5000")
    print(f"[+] Listening for incoming connections...")
    print("="*50)
    app.run(host="0.0.0.0", port=5000, debug=False)
