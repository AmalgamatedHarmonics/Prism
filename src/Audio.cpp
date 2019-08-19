#include "Rainbow.hpp"

using namespace rainbow;

float Audio::generateNoise() {
	float nO;
	switch (noiseSelected) {
		case 0:
			nO = brown.next() * 10.0f - 5.0f;
			break;
		case 1:
			nO = pink.next() * 10.0f - 5.0f;
			break;
		case 2:
			nO = white.next() * 10.0f - 5.0f;
			break;
		default:
			nO = pink.next() * 10.0f - 5.0f;
	}
	return nO;
}

void Audio::nChannelProcess(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output) {

	int chan;

	// Must generate 2, 3 or 6 channels
	switch(inputChannels) {
		case 0:
		case 1:
		case 2:
			chan = 2;
			break;
		case 3:
			chan = 3;
			break;
		default:
			chan = 6;
	}

	for (int i = 0; i < chan; i++) {
		if (!nInputBuffer[i].full()) {
			if (inputChannels == 0) {
				nInputFrame[i].samples[0] = generateNoise() / 5.0f;
			} else if (inputChannels == 1) {
				nInputFrame[i].samples[0] = input.getVoltage(0) / 5.0f;
			} else {
				nInputFrame[i].samples[0] = input.getVoltage(i) / 5.0f;
			}
			nInputBuffer[i].push(nInputFrame[i]);
		} 
	}

	// At this point we have populated 2,3 or 6 buffers

	// Process buffer
	if (outputBuffer.empty()) {

		for (int i = 0; i < chan; i++) {
			nInputSrc[i].setRates(sampleRate, 96000);

			int inLen = nInputBuffer[i].size();
			int outLen = NUM_SAMPLES;
			nInputSrc[i].process(nInputBuffer[i].startData(), &inLen, nInputFrames[i], &outLen);
			nInputBuffer[i].startIncr(inLen);

			for (int j = 0; j < NUM_SAMPLES; j++) {
				int32_t v = (int32_t)clamp(nInputFrames[i][j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);

				switch(chan) {
					case 2:
						main.io->in[i][j] 		= v;
						main.io->in[2 + i][j] 	= v;
						main.io->in[4 + i][j] 	= v;
						break;
					case 3:
						main.io->in[i][j] 		= v;
						main.io->in[1 + i][j] 	= v;
						break;
					default:
						main.io->in[i][j] 		= v;
				}
			}
		}

		// Pass to module
		main.process_audio();

		// Convert output buffer
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames[i].samples[chan] = main.io->out[chan][i] / MAX_12BIT;
			}
		}

		outputSrc.setRates(96000, sampleRate);
		int inLen = NUM_SAMPLES;
		int outLen = outputBuffer.capacity();
		outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
		outputBuffer.endIncr(outLen);
	}

	// Set output
	if (!outputBuffer.empty()) {
		outputFrame = outputBuffer.shift();

		float mono = 0.0f;
		float l = 0.0f;
		float r = 0.0;
		
		switch(outputChannels) {
			case 0:
				output.setChannels(1);
				break;
			case 1:
				output.setChannels(2);
				break;
			case 2:
				output.setChannels(6);
				break;
			default:
				output.setChannels(1);
				break;
		}

		for (int i = 0; i < NUM_CHANNELS; i++) {
			switch(outputChannels) {
				case 0:
					mono += outputFrame.samples[i];
					break;
				case 1:
					if (i & 1) {
						r += outputFrame.samples[i];
					} else {
						l += outputFrame.samples[i];
					}
					break;
				case 2:
					output.setVoltage(outputFrame.samples[i] * 5.0f, i);
					break;
				default:
					mono += outputFrame.samples[i];
					break;
			}
		}

		switch(outputChannels) {
			case 0:
				output.setVoltage(mono * 5.0f, 0);
				break;
			case 1:
				output.setVoltage(l * 5.0f, 0);
				output.setVoltage(r * 5.0f, 1);
				break;
			case 2: // Do nothing
				break;
			default:
				output.setVoltage(mono * 5.0f, 0);
				break;
		}
	}

}
