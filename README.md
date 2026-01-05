# vsp

vsp is a lightweight OpenGL audio visualizer, which captures your system audio and displays the spectrum. The yellow-on-black colour scheme is a part of its heritage. It's a port of [rvsp](https://github.com/cynthia2006/rvsp), but improved in many ways. 

[CinematicVSP.webm](https://github.com/user-attachments/assets/c3e6a155-3f7c-4178-bced-53face6879ce)

<div align="center">(Music: <i>Meltt — Within You, Within Me</i>)</div> 

## Installation

### Prerequisites

- Meson (build system)
- CMake (for Meson; more specifically, for KissFFT)
- GLFW 3.0
- PipeWire ≥0.3

```
$ git clone --recurse-submodules https://github.com/cynthia2006/vsp
$ cd vsp
$ meson setup builddir --buildtype=release
$ meson compile -C builddir
```

The final artifact would be `vsp` in `builddir`—takes no options, runs out of the box.

## Controls

- <kbd>↑</kbd> to increase and <kbd>↓</kbd> to decrease gain of the spectrum.
- <kbd>←</kbd> to decrease and <kbd>→</kbd> to increase smoothing time constant (0 < τ < 1).


### "Suckless" approach

This app is as bare minimum as it could get, and the defaults are universally the best choice. However, if your needs are special you can of course adjust the options by editing the code itself (options in `vsp.c`). There is no mechanism for loading configuration files, as the code needed for that would alone outweigh the existing codebase. In the Linux community this is known as the "suckless" approach.


### Smoothing time constant

Smoothing time constant (**τ**) is a parameter controlling [temporal smoothing of spectrum](https://en.wikipedia.org/wiki/Exponential_smoothing); higher the values the smoother the animation. The default is 0.8, which is quite eye-pleasing; lower values (typically around 0.6) are good for high-BPM music if you're into that.

## Spectrum

A [Mel-scale](https://en.wikipedia.org/wiki/Mel_scale) spectrum (20-20000 Hz) is used to display lower frequencies in greater detail, and higher frequencies in coarse detail. Amplitude is in linear scale however, simply because it's visually appealing. You usually adjust the gain of the spectrum using the <kbd>↑</kbd> and <kbd>↓</kbd> keys as needed (e.g. the volume of music is too low).

# Why switch from Rust to C?

Rust is not the magic bullet. A polished turd is still a turd; so no matter how much safety it tries to guarantee, the programmer is still liable for erroneous logic. Welcome to Rust bindings: the darkest secrets that rustaceans don't want you to see. “Fearlessness” in Rust is often taken for granted, and relying on these assumptions with binding crates turns out to be — more often than not — catastrophic, especially if the documentation is sparse.
## What bindings are for?
Not all libraries and applications have been “rewritten into Rust” even if rustaceans demand so. As a result, hard-to-implement frameworks require bindings because the manoeuvre is too large to reimplement it; it wouldn't be as well-known or robust as the original framework. 
## What bindings do wrong?
C interoperability aspect of Rust is poor compared to other languages (e.g. Zig). If someone isn't cautious during development it could cause hard-to-debug errors. The user of the binding is then left with a mere “illusion of safety” more dangerous than the unsafe interface (FFI) itself. When a malfunction occurs, the user is forced to read the source code, compare and contrast the behaviour of the abstraction with equivalent code leveraging FFI. The result? A huge time waste.

> TL;DR Unsafe Rust is equivalent to unsafe sex — two dangerous thrills of life.
## Suboptimal bindings
No abstraction is zero-cost, even if the overhead is infinitesimal. Bindings should mirror the C API in a one-to-one fashion as much as possible, even if it's not aligned with the idioms of the language. Making spurious allocations on behalf, incomplete coverage, use of suboptimal algorithms introduce a tradeoff between ease of use and performance.

Enough abstract ramblings. The real reason I abandoned [rvsp]() is due [pipewire-rs]() — a hot pile of garbage. PipeWire is the vital component here, and can't be traded off for anything else. I could have used PulseAudio however, but it's deprecated in the modern Linux stack. 

The secondary reason were the complexities that arose with using **winit** and **glutin** — all to initialise an OpenGL context, and **glium** being abandoned as early as 2016. A comprehensive tutorial for **winit** and **glutin** did not exist. A user was required to sit through, and read [an 600-700 LOC example](https://github.com/rust-windowing/glutin/blob/master/glutin_examples/src/lib.rs) to draw a triangle “portably” across devices using OpenGL. However, glium integrated quite well these two pair of libraries; **winit** created a window, **glutin** initialised the OpenGL context, and **glium** used the context. Though glium still did work, it wasn't actively developed; chances of code rot was high. Besides, it abstracted away too much details of the OpenGL API that there was no one-to-one mapping, so a user would have to translate between two paradigms — a time-waste. **glium** hadn't been free of bugs; it's original author left the project because many of OpenGL's quirks across GPU vendors.

At this point, I wasn't enjoying coding it; things became painful, and caused me a lot of sleepless nights. It not only took a mental, but a physical toll on me (not exaggerating).

I know, I could have opted for simpler alternatives such as GLFW or SDL2, but even with this sorted out, the primary criterion of capturing the system audio (from the default sink) was satisfied neither with SDL2 nor with pipewire-rs. I'd have to leverage the PipeWire C API directly, because the Rust bindings were garbage; both SDL and GLFW are C libraries as well, so the benefits of using Rust.
# Down the history lane
Initially, it was conceived to be a Python project; was intended to generate visualisations of audio files. It was slow, even though the drawing was done with [skia-python](https://github.com/kyamagu/skia-python), FFT done with [pyFFTW](https://github.com/pyFFTW/pyFFTW), and video encoding done with FFmpeg. I considered switching to Rust, but that plan failed as well — the speeds were equivalent, so I abandoned it altogether.

Eventually, I discovered that I had been using the CPU backend (software rasterizer) of Skia, which is ought to be slow. I got a novel idea, and that idea was integrated into **rvsp**, which reused parts of the abandoned code (especially spectral analysis and plotting). **rvsp** was an even older project; used SDL's native renderer for the same task instead of Skia, which resulted in ugly jagged lines (not anti-aliased). Leveraging the Ganesh (GPU) backend of Skia turned out to be a hectic job, because at that time I lacked knowledge of OpenGL, and the example code had used winit and glium, whose knowledge I also lacked. Nonetheless, after a painful trial-and-error procedure, I was able to get it working.
## Dicthing Skia
Skia was a heavy dependency, and compiling it took an enormous amount of time. It wasn't worth it because I was doing nothing advanced; just drawing anti-aliased lines. I found out that using OpenGL directly with a vertex shader (that passed vertices as is) and fragment shader (that uniformly coloured all pixels), instructing it to draw lines was enough.
## Switch to glium
OpenGL is a C library, thus requires unsafe for FFI. As unsafe Rust is treated like unsafe sex in the context of Rust programs, I sought an alternative; found one — glium. At first it seemed like the magic bullet, but I was in for a disappointment. During that time I was still using SDL, and I wanted to keep it intact, so I resisted the allure of seamless integration with winit and glium, and chose the hard path of implementing the backend [by implementing the required traits](https://docs.rs/glium/latest/glium/backend/index.html). It took me an embarrassingly long time to figure out how the traits interacted with eachother, prompting me to study the code. Eventually, I came up with [a solution](https://github.com/cynthia2006/rvsp/blob/3f0da4c7f8f5a314e9e5d892c22df2903bc3684b/src/sdl_backend.rs).
## Ditching SDL
One of the offputing things about SDL's audio subsystem was how it lacked functionality to capture monitor devices necessary to capture the system audio. I was forced to use **pavucontrol** to change the input device to the system sink monitor. To my misfortune, pavucontrol broke and I was forced to use **qpwgraph**; with each cable unplug, it would reset itself back to capture the microphone again.

This was quite annoying. I left no stone upturned to fix it, but it's just how SDL works. Frustrated, I then took a radical decision — do away with SDL entirely. The audio capturing functionality would be provided by PipeWire; window creation and OpenGL context creation would be provided by winit and glutin, respectively. 

I chose to break cross platform compatibility, because I want it to be exclusive to Linux — just as how Docker is — for the perverse pleasure of depriving BSD, Windows and Mac users from using it. It could be considered a form of revenge on Windows folks for depriving Linux users to use their apps without a translation layer (e.g. Wine or Steam Proton).
## Circleback to OpenGL
The real struggle began now. winit and glutin, as mentioned before, lack comprehensive tutorials, so I was forced to study the example code and apply it in practice. It was painful; if I knew beforehand, I would halt development that point. I leave out the details to the imagination as it could as well be the plot of a new snuff film.
## Struggles with pipewire-rs
pipewire-rs is unable to guarantee feature parity with C API of PipeWire. The support for threadloops has been [severely broken](https://gitlab.freedesktop.org/pipewire/pipewire-rs/-/issues/17), causing programs to crash non-deterministically. This means that the PipeWire's event loop is unable to run independently of the main thread occupied by winit's event loop. As a consequence, I was forced to either iterate the PipeWire's event loop it in the [`about_to_wait()`](https://docs.rs/winit/latest/winit/application/trait.ApplicationHandler.html#method.about_to_wait) callback of winit's event loop, or to iterate it when an event was available on the [event loop's file descriptor](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.fd) (as demonstrated [here](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/examples/gmain.c?ref_type=heads#L65-67)). 

Initially, I went for the first option; however the problem was if winit's event loop waited indefinitely for an event to arrive, it would stall PipeWire's event loop, blocking it from processing events arrived on its file descriptor — a suboptimal solution. Natrually, the second option was pursued; though, unfortunately `winit` doesn't expose a method to add a file descriptor to its [watch list](https://github.com/rust-windowing/winit/issues/3592), and notify when events arrive on it. At this point, the control had to be inverted. Luckily, PipeWire's event loop implementation allowed for it with [`add_io()`](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRc.html#method.add_io). The file descriptor of winit's event loop was added to the watch list of PipeWire's event loop, and the issue was likely resolved.

However, it wasn't just this issue. Another jarring issue was that audio capture configuration required to have mono channel layout, but instead of downmixing the stereo signal from the monitor of system default sink, it instead supplied the first channel (left). The channels were manually downmixed in the program. I thought there was a bug in PipeWire, but I soon met with surprise as this wasn't an issue with those programs that used the C API, and the developers couldn't reason about the potential cause. 

This ticked off the bomb, and I departed the land of rustaceans. Moral of the story: **all that glitters is not gold**. C might not be the most glittering language today, but it has stood the test of time, and has achieved immortality. Long live C!

The C API downmixes the channels appropriately, but `pipewire-rs` binding for a strange reason, doesn't; so [I had to implement it myself](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L101-L111). And, this last issue was enough for me to drive away from development of **rvsp**.
