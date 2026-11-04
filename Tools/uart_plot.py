"""Plot 16 nilai sensor + posisi dari UART.

Format baris dari firmware (debug.h):
    S,v0,v1,...,v15,pos
Contoh pakai:
    pip install pyserial matplotlib
    python uart_plot.py COM5 115200
"""
import sys

import matplotlib.pyplot as plt
import serial


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    ser = serial.Serial(port, baud, timeout=1)

    plt.ion()
    fig, ax = plt.subplots()
    bars = ax.bar(range(16), [0] * 16)
    ax.set_ylim(0, 4095)
    ax.set_xlabel("Channel")
    title = ax.set_title("pos = -")

    while plt.fignum_exists(fig.number):
        line = ser.readline().decode(errors="ignore").strip()
        parts = line.split(",")
        if len(parts) != 18 or parts[0] != "S":
            continue
        try:
            vals = [int(x) for x in parts[1:17]]
            pos = int(parts[17])
        except ValueError:
            continue
        for b, v in zip(bars, vals):
            b.set_height(v)
        title.set_text(f"pos = {pos}")
        plt.pause(0.001)


if __name__ == "__main__":
    main()
