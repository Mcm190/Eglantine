#!/usr/bin/env python3
"""Own the local Duskbloods servers for the lifetime of an emulator's stdin pipe."""

import json
from pathlib import Path
import runpy
import shutil
import signal
import sys
import tempfile


def main():
    config = json.loads(Path(sys.argv[1]).read_text())
    module = runpy.run_path(config['launcher'], run_name='dusk_launcher')
    state = Path(config['state'])
    state.mkdir(parents=True, exist_ok=True)
    source_pki = Path(config['pki'])
    if not (state / 'pki').exists() and source_pki.is_dir():
        shutil.copytree(source_pki, state / 'pki')
    run = Path(tempfile.mkdtemp(prefix='run-', dir=state))
    (run / 'server_data').mkdir()
    session = module['Session'](
        Path(), Path(sys.argv[0]),
        lambda message, error=False: print(message, file=sys.stderr, flush=True),
    )
    session.test = state

    def interrupted(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    try:
        print(f'Duskbloods server logs: {run}', file=sys.stderr, flush=True)
        if not session.start_servers(run):
            return 1
        env = {
            'EDEN_NX2_ONLINE_SERVER': '127.0.0.1:18443',
            'EDEN_NX2_ONLINE_CA': str(state / 'pki/ca.der'),
            'EDEN_NX2_ONLINE_HOSTS': 'npln.srv.nintendo.net,npln.nintendo.net,'
                                     'n.n.srv.nintendo.net,p.srv.nintendo.net,fromsoftware-game.net',
            'EDEN_NX2_LOOPBACK_ONLY': '1',
            'EDEN_NX2_DIAG_CODEC_CLIENT_REPLY': '1',
            'EDEN_NX2_EXPERIMENTAL_CODEC_DECODE': '1',
            'EDEN_NX2_EXPERIMENTAL_SYSMIX_OUTPUT': '1',
            'EDEN_NX2_SYSMIX_DRAIN_MULTIPLIER': '1',
            'EDEN_NX2_SILENT_BAD_OPUS': '1',
        }
        print(json.dumps(env), flush=True)
        # EOF also arrives when the emulator crashes or is killed.
        while sys.stdin.buffer.read(1):
            pass
        return 0
    except KeyboardInterrupt:
        return 130
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        session.stop()


if __name__ == '__main__':
    sys.exit(main())
