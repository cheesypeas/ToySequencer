void ControllerOutputEvent(MidiEvent event);
void ControllerOutputChannelOnOff(uint8_t channel, bool on);
void PianoKeyInput(uint8_t key, bool state);
void JoystickInput(uint8_t key, bool state);
void MainButtonsInput(uint8_t key, bool state);
void OtherButtonsInput(uint8_t key, bool state);

void ControllerInit();
