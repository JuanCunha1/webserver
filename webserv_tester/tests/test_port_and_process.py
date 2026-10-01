"""The evalsheet's "Port issues" section: multiple interfaces/ports, two
servers sharing host:port (with/without server_name), and two separate
webserv PROCESSES fighting over the same port."""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib import httpclient as hc
from lib import procman

HOST = "127.0.0.1"


def run(rec, ctx):
    rec.group("Port issues: multi-port, shared host:port, competing processes")

    # 1. Two independent webserv PROCESSES, two different config files,
    #    both trying to listen on the same port.
    proc_a = procman.WebservProcess(
        ctx["binary"], "configs/portconflict_a.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "portconflict_a.log"), host=HOST, ports=(8550,))
    started_a = proc_a.start()
    if not started_a:
        rec.crash("start first instance on port 8550", proc_a.read_log_tail())
        return
    try:
        rc, out, err = None, "", ""
        try:
            p = subprocess.run([ctx["binary"], "configs/portconflict_b.conf"],
                                cwd=ctx["tester_dir"], capture_output=True, text=True, timeout=2.0)
            rc, out, err = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired:
            rc = None

        rec.check(
            "second webserv process on an ALREADY-BOUND port exits cleanly (non-zero), doesn't hang",
            rc is not None and rc != 0,
            "rc=%r" % rc, "rc=%r stdout=%r stderr=%r (process had to be killed if rc is None)" % (rc, out, err))

        proc_a.assert_alive()
        rec.ok("the FIRST instance keeps running fine and is unaffected by the conflicting second one")
        r = hc.request(HOST, 8550, "GET", "/", headers={"Host": "localhost"}, timeout=2.0)
        rec.check("first instance still serves requests normally after the conflict attempt",
                  r.status_code == 200, "", repr(r))
    except procman.CrashError:
        rec.crash("first webserv instance crashed after a port-conflict attempt", proc_a.read_log_tail())
    finally:
        proc_a.stop()

    # 2. Two server{} blocks, ONE process, sharing host:port, WITH distinct
    #    server_name: allowed by the project's own config validator (subject:
    #    "virtual host is out of scope, but allowed if you want"). Actually
    #    binding the same host:port twice from one process typically still
    #    fails at the socket layer unless the group built real vhost dispatch
    #    on a single shared socket -- either outcome is fine, a CRASH is not.
    proc_v = procman.WebservProcess(
        ctx["binary"], "configs/vhost_ok.conf", ctx["tester_dir"],
        os.path.join(ctx["log_dir"], "vhost_ok.log"), host=HOST, ports=())
    ok = proc_v.start(wait_ports=False)
    time.sleep(0.5)
    if ok and proc_v.is_alive():
        rec.info("two server{} blocks sharing host:port (distinct server_name) started successfully",
                 "this group implemented real virtual-host dispatch on one shared listening socket")
        proc_v.stop()
    else:
        rc = proc_v.returncode()
        rec.check(
            "two server{} blocks sharing host:port (distinct server_name): if the group has no "
            "real vhost dispatch, the second bind() fails and the process must still exit CLEANLY "
            "(non-crash) rather than hang or segfault",
            rc is not None and rc >= 0,
            "rc=%r" % rc, proc_v.read_log_tail())
