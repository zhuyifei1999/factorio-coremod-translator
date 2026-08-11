# SPDX-License-Identifier: GPL-2.0

import asyncio
import socket
import struct
import sys
import traceback

import factoriotranslatorc


CONSOLE_DEST_LANG = 'en'
TEXTBOX_DEST_LANG = 'ko'


# HACK: Import native extenions from zip with memfd
def load_zipapp_natives():
    import importlib
    import os
    import zipfile

    # Need to have different fds so python doesn't reuse.
    memfds = []

    try:
        with zipfile.ZipFile(sys.path[0], 'r') as myself:
            for sofile in myself.namelist():
                if not sofile.endswith('.so'):
                    continue

                path = sofile.split('/')
                path[-1] = path[-1].split('.', 1)[0]
                module_name = '.'.join(path)

                fd = os.memfd_create(f'libfactoriotranslate.so:{sofile}')
                f = os.fdopen(fd, 'wb')
                memfds.append(f)
                f.write(myself.read(sofile))

                loader = importlib.machinery.ExtensionFileLoader(
                    module_name, f'/proc/self/fd/{fd}')
                spec = importlib.util.spec_from_loader(module_name, loader)

                # Need to lazy load because GRPC has circular dependency
                loader = importlib.util.LazyLoader(spec.loader)
                spec.loader = loader

                module = importlib.util.module_from_spec(spec)
                sys.modules[module_name] = module
                spec.loader.exec_module(module)
    finally:
        [f.close() for f in memfds]


load_zipapp_natives()


import google.cloud.translate_v2
import async_lru


sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')

endpoint = socket.fromfd(factoriotranslatorc.getfd(),
                         socket.AF_UNIX, socket.SOCK_STREAM)
endpoint.setblocking(False)

translate_client = google.cloud.translate_v2.Client()


@async_lru.alru_cache(maxsize=512)
async def translate(str, dest):
    result = await asyncio.to_thread(lambda: translate_client.translate(
        values=str,
        target_language=dest,
        format_='text',
    ))
    return result['translatedText']


async def translate_console(data):
    if ': ' not in data:
        # Not a message? Don't translate it
        print(data)
        return

    user, message = data.split(': ', 1)

    try:
        message_translated = await translate(message, dest=CONSOLE_DEST_LANG)
    except Exception:
        print(f'Exception translating {message}', file=sys.stderr)
        traceback.print_exc()
        return

    if message_translated == message:
        print(f'{user}: {message}')
        return

    newstr = f'{message} -> {message_translated}'
    print(f'{user}: {newstr}')

    # Wait for localization to complete
    await asyncio.sleep(0.1)

    factoriotranslatorc.apply_translation_to_console(
        message.encode('utf-8', errors='surrogateescape'),
        newstr.encode('utf-8', errors='surrogateescape'),
    )


async def translate_textbox(data):
    try:
        translated = await translate(data, dest=TEXTBOX_DEST_LANG)
    except Exception:
        print(f'Exception translating {data}', file=sys.stderr)
        traceback.print_exc()
        factoriotranslatorc.textbox_translate_error()
        return

    if translated == data:
        print(f'{data} cannot be translated', file=sys.stderr)
        factoriotranslatorc.textbox_translate_error()
        return

    try:
        reversed = await translate(translated, dest=CONSOLE_DEST_LANG)
    except Exception:
        print(f'Exception translating {translated}', file=sys.stderr)
        traceback.print_exc()
        factoriotranslatorc.textbox_translate_error()
        return

    src = data
    dest = translated
    explain = f'{data} -> {translated} -> {reversed}'

    print(f'TextBox: {explain}')

    factoriotranslatorc.apply_translation_to_textbox(
        src.encode('utf-8', errors='surrogateescape'),
        dest.encode('utf-8', errors='surrogateescape'),
        explain.encode('utf-8', errors='surrogateescape'),
    )


async def receiver_taskfn():
    loop = asyncio.get_running_loop()

    while True:
        len_msg = await loop.sock_recv(endpoint, 9)
        cmd, length, = struct.unpack('=BQ', len_msg)

        data = b''
        while length:
            pkt = await loop.sock_recv(endpoint, length)
            data += pkt
            length -= len(pkt)

        data = data.decode('utf-8', errors='surrogateescape')

        if cmd == 0:
            await translate_console(data)
        elif cmd == 1:
            await translate_textbox(data)


async def main():
    tasks = [
        asyncio.create_task(receiver_taskfn())
    ]

    [await task for task in tasks]

asyncio.run(main())
