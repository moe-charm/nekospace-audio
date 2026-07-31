# SPDX-FileCopyrightText: 2026 TextureVoice
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Dump the metadata of a SOFA file so the conversion assumptions can be checked."""
import sys
import h5py
import numpy as np

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main(path):
    with h5py.File(path, "r") as f:
        print("== global attributes ==")
        for k, v in f.attrs.items():
            s = v.decode() if isinstance(v, bytes) else str(v)
            if len(s) > 300:
                s = s[:300] + " ...[truncated]"
            print(f"  {k} = {s}")

        print("\n== datasets ==")
        for k in f.keys():
            d = f[k]
            print(f"  {k}: shape={d.shape} dtype={d.dtype}")
            for ak, av in d.attrs.items():
                sv = av.decode() if isinstance(av, bytes) else str(av)
                print(f"      @{ak} = {sv}")

        sr = np.array(f["Data.SamplingRate"]).ravel()
        pos = np.array(f["SourcePosition"])
        ir = f["Data.IR"]
        print(f"\nsample rate      : {sr}")
        print(f"IR shape (M,R,N) : {ir.shape}")
        print(f"SourcePosition   : {pos.shape}")
        print(f"  azimuth  range : {pos[:,0].min():.2f} .. {pos[:,0].max():.2f}")
        print(f"  elevation range: {pos[:,1].min():.2f} .. {pos[:,1].max():.2f}")
        print(f"  radius   range : {pos[:,2].min():.3f} .. {pos[:,2].max():.3f}")

        # Sanity: which ear is loud for a source clearly on the listener's left?
        # SOFA SimpleFreeFieldHRIR azimuth is counter-clockwise, so +90 = left.
        idx = int(np.argmin(np.abs(pos[:, 0] - 90.0) + np.abs(pos[:, 1])))
        d = np.array(ir[idx])
        e0, e1 = float(np.sum(d[0] ** 2)), float(np.sum(d[1] ** 2))
        print(f"\nnearest point to SOFA az=+90, el=0 -> index {idx} "
              f"(az={pos[idx,0]:.2f}, el={pos[idx,1]:.2f})")
        print(f"  receiver0 energy = {e0:.6e}")
        print(f"  receiver1 energy = {e1:.6e}")
        print(f"  louder receiver  = {0 if e0 > e1 else 1} "
              f"(expected 0 = left ear, since SOFA +90 az is the left side)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "external/hrtf-data/HRIR_L2702.sofa")
