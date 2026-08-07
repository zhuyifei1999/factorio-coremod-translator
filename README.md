# Factorio-coremod-translator

Factorio in-game real-time chat translations, held together by magic and
duct tapes.

Since this "mod" is doing the doing the equivalent of DLL injection, it's
not something I can publish to mod portal. Hence "coremod".

Only client needs to install this. This mod is multiplayer-compatible and
has no effect on other players.

Linux only because I don't know other platforms well enough. (Sorry!)
Only the standalone version of the game is tested, not the Steam version.

## Usage

You need to setup gcloud auth so this can access Google Cloud Translation API:  
https://docs.cloud.google.com/translate/docs/reference/libraries/v2/overview-v2  
(You can change to a different library if you want, but I haven't tested them)

Compile:
```
make -j$(nproc)
```

Load into game:
```
LD_PRELOAD=/path/to/libfactoriotranslate.so /path/to/factorio/bin/x64/factorio
```

With mimalloc:
```
LD_PRELOAD='/path/to/libmimalloc.so /path/to/libfactoriotranslate.so' /path/to/factorio/bin/x64/factorio
```

To change target language edit `py/factoriotranslator.py` and recompile.

To translate the text you entered into chat press `CTRL-.` twice (first to
begin the translation, second to display the translation and back-translation
in the chat box). To revert, `Ctrl-,`. To confirm, either press `ENTER` (which
will confirm the translation right before the message is sent) or press
`CTRL-/` (which will not send the messsage, allowing you to edit it before
sending).

## License

Copyright (C) 2026 YiFei Zhu

This program is free software; you can redistribute it and/or modify  
it under the terms of the GNU General Public License as published by  
the Free Software Foundation; either version 2 of the License, or  
(at your option) any later version.  

This program is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of  
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along  
with this program; if not, write to the Free Software Foundation, Inc.,  
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
