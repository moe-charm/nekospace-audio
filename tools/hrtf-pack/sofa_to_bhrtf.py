# SPDX-FileCopyrightText: 2026 TextureVoice
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Convert a SimpleFreeFieldHRIR SOFA file into a NekoSpace .bhrtf pack.

The engine synthesises ITD geometrically from the rigid-sphere path, so the packed
HRIRs must carry no delay of their own or the two would add up. Each HRIR is therefore
converted to **minimum phase**, which removes all delay while preserving the magnitude
response exactly, and concentrates the energy at the start so truncation is clean.

Levels are NOT normalised here: the engine matches the pack to the analytic profile at
load time, so the analytic model stays the single source of truth for reference level.

Usage:
    python sofa_to_bhrtf.py <input.sofa> <output.bhrtf>
"""
import struct
import sys

import h5py
import numpy as np

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# Must match HrtfDatabase in src/dsp/HrtfDatabase.h
NUM_AZ, NUM_EL = 72, 13
AZ_STEP, EL_MIN, EL_STEP = 5.0, -90.0, 15.0
MAGIC = b"NSBH"
FORMAT_VERSION = 1
NEIGHBOURS = 3


def unit_vector(az_deg, el_deg):
    """Engine frame: x = right, y = up, z = front; azimuth positive to the right."""
    az, el = np.radians(az_deg), np.radians(el_deg)
    ce = np.cos(el)
    return np.stack([np.sin(az) * ce, np.sin(el), np.cos(az) * ce], axis=-1)


def sofa_unit_vector(az_deg, el_deg):
    """SOFA SimpleFreeFieldHRIR: azimuth counter-clockwise, so positive azimuth is the
    listener's LEFT. Mirrored into the engine frame here, once, so nothing downstream
    has to reason about the sign."""
    az, el = np.radians(az_deg), np.radians(el_deg)
    ce = np.cos(el)
    return np.stack([-np.sin(az) * ce, np.sin(el), np.cos(az) * ce], axis=-1)


def minimum_phase(h, nfft=4096):
    """Real-cepstrum minimum-phase reconstruction. Same magnitude, no delay."""
    spec = np.fft.rfft(h, nfft)
    mag = np.abs(spec)
    mag = np.maximum(mag, mag.max() * 1e-7)          # floor keeps log finite
    log_mag = np.log(mag)
    full = np.concatenate([log_mag, log_mag[-2:0:-1]])
    cepstrum = np.fft.ifft(full).real
    fold = np.zeros(nfft)
    fold[0] = 1.0
    fold[1: nfft // 2] = 2.0
    fold[nfft // 2] = 1.0
    return np.fft.ifft(np.exp(np.fft.fft(cepstrum * fold))).real


def main(src, dst):
    with h5py.File(src, "r") as f:
        ir = np.array(f["Data.IR"])                   # (M, R, N)
        pos = np.array(f["SourcePosition"])           # (M, 3) az, el, radius
        sr = float(np.array(f["Data.SamplingRate"]).ravel()[0])
        attrs = {k: (v.decode(errors="replace") if isinstance(v, bytes) else str(v))
                 for k, v in f.attrs.items()}

    m, receivers, n = ir.shape
    if receivers != 2:
        raise SystemExit(f"expected 2 receivers, found {receivers}")
    if attrs.get("SOFAConventions") != "SimpleFreeFieldHRIR":
        raise SystemExit(f"unsupported convention {attrs.get('SOFAConventions')!r}")

    print(f"source      : {src}")
    print(f"  {attrs.get('ListenerDescription', '?')} / {attrs.get('Author', '?')}")
    print(f"  licence    : {attrs.get('License', '?')}")
    print(f"  {m} directions, {n} taps, {sr:.0f} Hz, radius {pos[:,2].mean():.2f} m")

    taps = int(round(128 * sr / 48000.0))
    taps = int(np.clip(taps, 64, 256))

    # --- minimum phase, once per measured direction ---
    min_phase = np.empty((m, 2, taps), dtype=np.float64)
    for i in range(m):
        for r in range(2):
            min_phase[i, r] = minimum_phase(ir[i, r])[:taps]

    front = np.abs(ir).max()
    kept = np.abs(min_phase).max()
    print(f"min-phase   : peak {front:.4f} -> {kept:.4f}, "
          f"energy kept {np.sum(min_phase**2) / np.sum(ir**2):.4f}")

    # --- regrid onto the engine's direction grid ---
    src_vecs = sofa_unit_vector(pos[:, 0], pos[:, 1])
    out = np.zeros((NUM_EL, NUM_AZ, 2, taps), dtype=np.float32)
    max_gap = 0.0
    for ei in range(NUM_EL):
        el = EL_MIN + EL_STEP * ei
        for ai in range(NUM_AZ):
            az = AZ_STEP * ai
            target = unit_vector(az, el)
            # angular distance on the unit sphere
            cosang = np.clip(src_vecs @ target, -1.0, 1.0)
            nearest = np.argpartition(-cosang, NEIGHBOURS)[:NEIGHBOURS]
            ang = np.arccos(cosang[nearest])
            max_gap = max(max_gap, float(ang.min()))
            w = 1.0 / (ang + 1e-4)
            w /= w.sum()
            # safe to blend directly: the HRIRs are time-aligned (min phase)
            for r in range(2):
                out[ei, ai, r] = np.tensordot(w, min_phase[nearest, r], axes=(0, 0))
    print(f"regrid      : {NUM_AZ}x{NUM_EL} directions, "
          f"largest nearest-neighbour gap {np.degrees(max_gap):.2f} deg")

    # --- orientation self-check before anything is written ---
    ai90, ei0 = int(90.0 / AZ_STEP), int((0.0 - EL_MIN) / EL_STEP)
    e_left = float(np.sum(out[ei0, ai90, 0] ** 2))
    e_right = float(np.sum(out[ei0, ai90, 1] ** 2))
    print(f"orientation : engine az=+90 (right) -> L {e_left:.4e}  R {e_right:.4e}")
    if e_right <= e_left:
        raise SystemExit("ABORT: a source on the right is not louder in the right ear — "
                         "azimuth sign or receiver order is wrong")

    header = (MAGIC
              + struct.pack("<IIIIf", FORMAT_VERSION, NUM_AZ, NUM_EL, taps, sr)
              + struct.pack("<fff", EL_MIN, EL_STEP, AZ_STEP))
    with open(dst, "wb") as f:
        f.write(header)
        f.write(out.tobytes(order="C"))
    print(f"written     : {dst} ({len(header) + out.size * 4} bytes, "
          f"{len(header)}-byte header)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
