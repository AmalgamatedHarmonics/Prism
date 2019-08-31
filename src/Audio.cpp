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

void Audio::ChannelProcess1(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output) {

	int inChannels;

	// Must generate 2, 3 or 6 input streams
	switch(inputChannels) {
			case 0:
			case 1:
			case 2:
					inChannels = 2;
					break;
			case 3:
					inChannels = 3;
					break;
			default:
					inChannels = 6;
	}

	for (int i = 0; i < inChannels; i++) {
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
	if (outputBuffer1.empty()) {

		for (int i = 0; i < inChannels; i++) {
			nInputSrc[i].setRates(sampleRate, 96000);

			int inLen = nInputBuffer[i].size();
			int outLen = NUM_SAMPLES;
			nInputSrc[i].process(nInputBuffer[i].startData(), &inLen, nInputFrames[i], &outLen);
			nInputBuffer[i].startIncr(inLen);

			for (int j = 0; j < NUM_SAMPLES; j++) {
				int32_t v = (int32_t)clamp(nInputFrames[i][j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);

				switch(inChannels) {
					case 2:
						main.io->in[i][j] 			= v;
						main.io->in[2 + i][j] 		= v;
						main.io->in[4 + i][j] 		= v;
						break;
					case 3:
						main.io->in[i * 2][j] 		= v;
						main.io->in[1 + i * 2][j] 	= v;
						break;
					default:
						main.io->in[i][j] 			= v;
				}
			}
		}

		// Pass to module
		main.process_audio();

		// Convert output buffer
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames1[i].samples[0] = 0;
			}
		}

		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames1[i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
			}
		}

		outputSrc1.setRates(96000, sampleRate);
		int inLen = NUM_SAMPLES;
		int outLen = outputBuffer1.capacity();
		outputSrc1.process(outputFrames1, &inLen, outputBuffer1.endData(), &outLen);
		outputBuffer1.endIncr(outLen);
	}

	// Set output
	if (!outputBuffer1.empty()) {
		outputFrame1 = outputBuffer1.shift();
		output.setChannels(1);
		output.setVoltage(outputFrame1.samples[0] * 5.0f, 0);
	}

}

void Audio::ChannelProcess2(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output) {

	int inChannels;

	// Must generate 2, 3 or 6 input streams
	switch(inputChannels) {
			case 0:
			case 1:
			case 2:
					inChannels = 2;
					break;
			case 3:
					inChannels = 3;
					break;
			default:
					inChannels = 6;
	}

	for (int i = 0; i < inChannels; i++) {
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
	if (outputBuffer2.empty()) {

		for (int i = 0; i < inChannels; i++) {
			nInputSrc[i].setRates(sampleRate, 96000);

			int inLen = nInputBuffer[i].size();
			int outLen = NUM_SAMPLES;
			nInputSrc[i].process(nInputBuffer[i].startData(), &inLen, nInputFrames[i], &outLen);
			nInputBuffer[i].startIncr(inLen);

			for (int j = 0; j < NUM_SAMPLES; j++) {
				int32_t v = (int32_t)clamp(nInputFrames[i][j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);

				switch(inChannels) {
					case 2:
						main.io->in[i][j] 			= v;
						main.io->in[2 + i][j] 		= v;
						main.io->in[4 + i][j] 		= v;
						break;
					case 3:
						main.io->in[i * 2][j] 		= v;
						main.io->in[1 + i * 2][j] 	= v;
						break;
					default:
						main.io->in[i][j] 			= v;
				}
			}
		}

		// Pass to module
		main.process_audio();

		// Convert output buffer
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames2[i].samples[0] = 0;
				outputFrames2[i].samples[1] = 0;
			}
		}

		// Convert output buffer
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				if (chan & 1) {
					outputFrames2[i].samples[1] += main.io->out[chan][i] / MAX_12BIT;
				} else {
					outputFrames2[i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
				}
			}
		}

		outputSrc2.setRates(96000, sampleRate);
		int inLen = NUM_SAMPLES;
		int outLen = outputBuffer2.capacity();
		outputSrc2.process(outputFrames2, &inLen, outputBuffer2.endData(), &outLen);
		outputBuffer2.endIncr(outLen);
	}

	// Set output
	if (!outputBuffer2.empty()) {
		outputFrame2 = outputBuffer2.shift();
		output.setChannels(2);
		output.setVoltage(outputFrame2.samples[0] * 5.0f, 0);
		output.setVoltage(outputFrame2.samples[1] * 5.0f, 1);
	}

}

void Audio::ChannelProcess6(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output) {

	int inChannels;

	// Must generate 2, 3 or 6 input streams
	switch(inputChannels) {
			case 0:
			case 1:
			case 2:
					inChannels = 2;
					break;
			case 3:
					inChannels = 3;
					break;
			default:
					inChannels = 6;
	}

	for (int i = 0; i < inChannels; i++) {
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
	if (outputBuffer6.empty()) {

		for (int i = 0; i < inChannels; i++) {
			nInputSrc[i].setRates(sampleRate, 96000);

			int inLen = nInputBuffer[i].size();
			int outLen = NUM_SAMPLES;
			nInputSrc[i].process(nInputBuffer[i].startData(), &inLen, nInputFrames[i], &outLen);
			nInputBuffer[i].startIncr(inLen);

			for (int j = 0; j < NUM_SAMPLES; j++) {
				int32_t v = (int32_t)clamp(nInputFrames[i][j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);

				switch(inChannels) {
					case 2:
						main.io->in[i][j] 			= v;
						main.io->in[2 + i][j] 		= v;
						main.io->in[4 + i][j] 		= v;
						break;
					case 3:
						main.io->in[i * 2][j] 		= v;
						main.io->in[1 + i * 2][j] 	= v;
						break;
					default:
						main.io->in[i][j] 			= v;
				}
			}
		}

		// Pass to module
		main.process_audio();

		// Convert output buffer
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames6[i].samples[chan] = main.io->out[chan][i] / MAX_12BIT;
			}
		}

		outputSrc6.setRates(96000, sampleRate);
		int inLen = NUM_SAMPLES;
		int outLen = outputBuffer6.capacity();
		outputSrc6.process(outputFrames6, &inLen, outputBuffer6.endData(), &outLen);
		outputBuffer6.endIncr(outLen);
	}

	// Set output
	if (!outputBuffer6.empty()) {
		outputFrame6 = outputBuffer6.shift();
		output.setChannels(6);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			output.setVoltage(outputFrame6.samples[i] * 5.0f, i);
		}
	}
}
