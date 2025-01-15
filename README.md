## randwave~: Random single-period waveform generator in PureData

**This project is still a work-in-progress. Expect bugs, malfunctions, and generally bad code.**

*Created by Ian Doherty, December 2024 - January 2025*

This Pd external creates random single-period waveforms and plays them back for the user. A random waveform, in this context, is defined as a sequence of randomly-generated sample and amplitude pairs that are connected via trigonometric interpolation. All random values are generated using a uniform random distribution.

The creation of randwave~ was heavily inspired by Iannis Xenakis' [Dynamic Stochastic Synthesis (DSS)](https://en.wikipedia.org/wiki/Iannis_Xenakis) procedure, and attempts to emulate the process with more traditional waveforms. Additionally, the code for this project was influenced by Eric Lyon's dynstoch~ external, which implements DSS in Pd.

## Installation
1. If you haven't already, install PureData [here](https://puredata.info/).
2. Clone this repo using `git clone` on a command line or download the source code manually.
3. In the installation folder, run `make` on a Linux command line. For Windows users, consider using [MinGW](https://osdn.net/projects/mingw/) or [WSL](https://learn.microsoft.com/en-us/windows/wsl/install). For a system-wide installation, run `make install`.
4. Open `randwave~-help.pd` to learn the usage for randwave~. If `make install` was used, you will be able to add randwave~ objects into any PureData instance.
	* *To view this in PureData, go to  Help > Browser. randwave~ should be listed.*

## Credits & Libraries Used
* [PureData](https://puredata.info/): Everything!
* [pure-data/pd-lib-builder](https://github.com/pure-data/pd-lib-builder): Makefile generation
* [pure-data/externals-howto](https://github.com/pure-data/externals-howto?tab=readme-ov-file#atom-string): Helpful guide used during the creation of this external