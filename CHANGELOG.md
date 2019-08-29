# Prism change log

## Rainbow

1.1.0
* Complete UI overhaul by Pyer
* Polyphonic audio input and output, with 1,2 and 6 channels in and out
* Added Spectrum - user scale creator for Rainbow
* Per-channel mono input for Level and Q, and output for V/Oct and Envelope
* V/Oct tracking for BpRe mode - just the basic filter tuning, not frequency nudge or V/Oct input
* The final calculated input levels are echoed on Poly Env out channels 7-12
* Refactoring scale data with additional bank and scale information
* Recalculate Mesopotamian scale
* Restore unused scales from SMR
* Update trigger calculation
* Revise tuning LED colour scheme
* Merge clip and level LEDs
* Freq CV now plus/minus 5V, 1V/Oct
* Nudge CV now +-1 semitone
* Transpose now +- 1 octave in semitone steps
* A polyphonic input to the FREQ CV1 port addresses all channels individually, but input does not pass through a LPF

1.0.2
* Split Level and Q CV inputs into separate global and channel inputs
* Add indicator for channel level
* Revise Level calculation
* Revise Q calculation
* Change Level LEDs to Q LEDs
* Resolve clipping problem
* Update UI text

1.0.1 
* Fix downsampling problem
* Remove compressor
* Update UI text

1.0.0
* First version

## Spectrum

1.1.0

* First version
