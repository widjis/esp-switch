# PlatformIO/SCons hook: C3 USB reset controls C3, not the target ESP8266.
Import("env")

def configure_upload(source, target, env):
    if env.GetProjectOption("upload_protocol", "esptool") != "esptool":
        return
    env.Replace(UPLOADERFLAGS=[
        "--chip", "esp8266", "--port", '"$UPLOAD_PORT"',
        "--baud", "$UPLOAD_SPEED", "--before", "no_reset",
        "--after", "no_reset", "write_flash"])

env.AddPreAction("upload", configure_upload)
