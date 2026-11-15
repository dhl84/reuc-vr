"""Sound check without ears: turns a UC_AUDIO_DUMP file into a WAV file, prints numbers for a WAV file, and finds a reference sound in it.

The game writes every block of mixed sound that it gives to the audio device to the file <prefix>.<pid> when UC_AUDIO_DUMP=<prefix> is set
(game/audio_output.cpp): {steady clock ns u64, bytes u32, queued bytes u32, pcm}, stereo, 16 bit, 32000 Hz. The dump also works with --mute and
at any speed of the game clock: the samples are game time (96 frames for each 3 ms).

Usage:
  python scripts/audio_check.py wav <dump> <out.wav>
  python scripts/audio_check.py stat <wav> [--from S] [--to S] [--window S]
  python scripts/audio_check.py find <capture.wav> <reference.wav> [--ref-from S] [--ref-len S] [--cap-from S] [--cap-to S]

stat prints for each window: RMS and peak in dBFS, the share of samples at full scale (clipping), the spectral centroid, the spectral
flatness (1 is white noise, near 0 is tones), the share of the energy below 300 Hz, from 300 to 3000 Hz, from 3000 to 8000 Hz and above,
the correlation of left and right, the mean (DC) and the zero crossings as a frequency.

find cross-correlates a segment of the reference (for example a stream file of the game decoded with ffmpeg: ffmpeg -i x.brstm x.wav) with the
capture, at the right rate and at wrong rates for comparison. A normalised correlation near 1 at the ratio 1.000 means that the capture holds
the same waveform at the right sample rate; the gain is capture / reference at that place. Other sounds that play at the same time lower
the correlation.

stat and find need numpy; find needs scipy.
"""
import argparse
import struct
import sys
import wave


def to_wav(src, dst):
    blocks = total = 0
    first = last = None
    with open(src, "rb") as f, wave.open(dst, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(32000)
        while True:
            head = f.read(16)
            if len(head) < 16:
                break
            now, size, _queued = struct.unpack("<QII", head)
            pcm = f.read(size)
            if len(pcm) < size:
                break
            first = now if first is None else first
            last = now
            w.writeframes(pcm)
            total += size // 4
            blocks += 1
    print("blocks %d frames %d (%.1f s of sound) host span %.1f s" % (blocks, total, total / 32000.0, ((last or 0) - (first or 0)) / 1e9))


def load(path, start=0.0, end=None):
    import numpy as np
    with wave.open(path, "rb") as w:
        rate, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        a = min(n, int(start * rate))
        b = n if end is None else min(n, int(end * rate))
        w.setpos(a)
        data = np.frombuffer(w.readframes(max(0, b - a)), dtype="<i2").reshape(-1, ch).astype(np.float64) / 32768.0
    return rate, data, n / rate


def numbers(rate, x):
    import numpy as np
    db = lambda v: 20 * np.log10(max(v, 1e-9))
    mono = x.mean(axis=1)
    rms, peak = np.sqrt((x ** 2).mean()), np.abs(x).max()
    out = dict(rms=db(rms), peak=db(peak), clip=float((np.abs(x) >= 32767 / 32768.0).mean()), centroid=0.0, flat=0.0, bands=(0.0, 0.0, 0.0, 0.0), lr=0.0, dc=0.0, zc=0.0)
    if rms < 1e-6:
        return out
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)))) ** 2
    freq = np.fft.rfftfreq(len(mono), 1.0 / rate)
    total = spec.sum() + 1e-30
    out["centroid"] = float((spec * freq).sum() / total)
    bins = np.add.reduceat(spec, np.searchsorted(freq, np.arange(100, 14000, 100)))[:-1]   # the power in 100 Hz steps from 100 Hz to 14 kHz
    out["flat"] = float(np.exp(np.log(bins + 1e-30).mean()) / (bins.mean() + 1e-30))
    edges = [0, 300, 3000, 8000, rate]
    out["bands"] = tuple(float(spec[(freq >= a) & (freq < b)].sum() / total) for a, b in zip(edges, edges[1:]))
    if x.shape[1] > 1 and x[:, 0].std() > 0 and x[:, 1].std() > 0:
        out["lr"] = float(np.corrcoef(x[:, 0], x[:, 1])[0, 1])
    out["dc"] = float(mono.mean())
    out["zc"] = float((np.diff(np.sign(mono)) != 0).mean() * rate / 2)
    return out


def stat(args):
    rate, _, length = load(args.wav, 0, 0)
    end = length if args.end is None else min(args.end, length)
    print("%s: %d Hz, %.1f s" % (args.wav, rate, length))
    print("  from     to  rms dB peak dB  clip %  centroid  flat  <300  300-3k  3k-8k   >8k   L/R      DC   zc Hz")
    t = args.start
    while t < end - 0.01:
        _, x, _ = load(args.wav, t, min(t + args.window, end))
        if len(x) < 64:
            break
        s = numbers(rate, x)
        print("%6.1f %6.1f %7.1f %7.1f %7.3f %9.0f %5.2f %5.2f %6.2f %6.2f %5.2f %5.2f %7.4f %7.0f" % (
            t, min(t + args.window, end), s["rms"], s["peak"], 100 * s["clip"], s["centroid"], s["flat"], *s["bands"], s["lr"], s["dc"], s["zc"]))
        t += args.window


def find(args):
    import numpy as np
    from scipy.signal import correlate, resample_poly

    def best(cap, ref):   # normalised cross-correlation of ref inside cap: (peak, offset in samples, gain)
        if len(ref) > len(cap):
            return 0.0, 0, 0.0
        corr = correlate(cap, ref, mode="valid", method="fft")
        energy = np.cumsum(np.concatenate(([0.0], cap ** 2)))
        ncc = corr / (np.sqrt((energy[len(ref):] - energy[:-len(ref)]) * (ref ** 2).sum()) + 1e-12)
        at = int(np.argmax(np.abs(ncc)))
        return float(ncc[at]), at, float(corr[at] / ((ref ** 2).sum() + 1e-12))

    crate, cap, _ = load(args.capture, args.cap_from, args.cap_to)
    rrate, seg, _ = load(args.reference, args.ref_from, args.ref_from + args.ref_len)
    print("capture %d Hz %.1f s, reference %d Hz, segment %.2f to %.2f s" % (crate, len(cap) / crate, rrate, args.ref_from, args.ref_from + len(seg) / rrate))
    mono = seg.mean(axis=1)
    for name, (up, down) in (("1.000", (1, 1)), ("0.980", (50, 49)), ("1.020", (49, 50)), ("0.667 (48000 Hz played as 32000)", (3, 2)), ("1.500", (2, 3)), ("0.726 (44100 Hz played as 32000)", (441, 320))):
        r = mono if up == down else resample_poly(mono, up, down)
        if rrate != crate:
            r = resample_poly(r, crate, rrate)
        peak, at, gain = best(cap.mean(axis=1), r)
        print("  ratio %-34s correlation %+.3f at capture %.3f s, gain %.3f" % (name, peak, args.cap_from + at / crate, gain))
    if seg.shape[1] == 2 and cap.shape[1] == 2 and rrate == crate:   # each channel against its own, and against the other one
        for i, label in ((0, "left"), (1, "right")):
            peak, at, gain = best(cap[:, i], seg[:, i])
            other = best(cap[:, i], seg[:, 1 - i])[0]
            print("  ratio 1.000 %-5s correlation %+.3f at capture %.3f s, gain %.3f (with the other channel of the reference: %+.3f)" % (label, peak, args.cap_from + at / crate, gain, other))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("wav")
    p.add_argument("dump")
    p.add_argument("out")
    p = sub.add_parser("stat")
    p.add_argument("wav")
    p.add_argument("--from", dest="start", type=float, default=0.0)
    p.add_argument("--to", dest="end", type=float, default=None)
    p.add_argument("--window", type=float, default=5.0)
    p = sub.add_parser("find")
    p.add_argument("capture")
    p.add_argument("reference")
    p.add_argument("--ref-from", type=float, default=0.0)
    p.add_argument("--ref-len", type=float, default=5.0)
    p.add_argument("--cap-from", type=float, default=0.0)
    p.add_argument("--cap-to", type=float, default=None)
    args = ap.parse_args()
    if args.cmd == "wav":
        to_wav(args.dump, args.out)
    elif args.cmd == "stat":
        stat(args)
    else:
        find(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
