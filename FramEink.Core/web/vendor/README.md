# Offline HEIC decoder

`heic-to-1.5.2.js.gz` is the **unmodified** browser ESM distribution
`package/dist/heic-to.min.js`, gzip compressed for storage in the ESP32 flash.
The browser downloads it from FramEInk only if its native image decoder cannot
open a HEIC/HEIF photo. No CDN, Internet connection, server conversion, or
additional SD files are needed.

- Project and corresponding source: https://github.com/hoppergee/heic-to/tree/v1.5.2
- Exact published archive: https://registry.npmjs.org/heic-to/-/heic-to-1.5.2.tgz
- Archive SHA-512 (base64):
  `8Fns+lZHAWmz5U5IUxDeXKwIf3foBoKNPLxxFY4B0MkLjNuomEIHCoDbDE+x/llFK3NCEO1cu4+n3iUKY+Svmw==`
- License declared by the package: LGPL-3.0; see `heic-to-LICENSE.txt` and
  the accompanying `GPL-3.0.txt` (https://www.gnu.org/licenses/gpl-3.0.txt).
- Included decoder: libheif 1.22.2, https://github.com/strukturag/libheif/tree/v1.22.2
- Included HEVC decoder: libde265 1.0.16 (as pinned in the upstream build guide),
  https://github.com/strukturag/libde265/tree/v1.0.16

The upstream repository includes the decoder sources/build instructions and the
standalone distributable, so it can be modified and replaced independently.
To replace or rebuild it, download the pinned archive and run:

```sh
python3 tools/build_portal_assets.py --import-heic /path/to/heic-to-1.5.2.tgz
```

For another version, update the source/archive links and integrity constant in
the generator, replace the decoder and its license, and regenerate the header.
`PortalAssets.h` is the generated flash representation. Arduino builds need no
Python invocation; regenerate only after changing portal sources or this vendor
asset. The decoder runs on the smartphone, never on the ESP32.
