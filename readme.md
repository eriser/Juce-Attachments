# Juce Attachments

This proposal features three Attachments for the juce::AudioProcessorValueTreeState class
- SbRadioGroupAttachment (to be used with the custom SbRadioGroup component) 
- LabelAttachment
- ComboBoxAttachment (based on the existing version)

The attachents make use of the user defined valueToTextFunction and textToValueFunction when set as the last 2 parameters from the createAndAddParameter function.

These functions are used to create an updated text entry from a new parameter value and to evaluate text entries in the label or comboBox editor. The SbRadioGroupAttachment will populate the RadioGroup with ToggleButtons based on the range property of the attached parameter.
It is also possible now to create the Combobox item list by connecting it to a parameter through the Attachment.

## The Demo Projects:

### PresetVsUserSettings
This example illustates some modifications I have made to the AudioProcessorValueTreeState class.
There are now two groups of Parameters that are handled differently:
 - Preset Parameters
   The state of these is saved with the preset.
 - User Settings
   Their state is loaded from a settings file when the processor is created. The file is saved whenever a user setting is changed and also when the processor is destroyed. It is recommended to disable host automation for this parameter group.
   
 Both preset parameters and user settings are published to the host and can be modified through the host interface if present. 

### RadioGroup
the RadioGroup example features a component that is populated with a group of radio buttons. The Buttons are created by a SbRadioGroupAttachment upon connection to the component.

### AttachmentsDemo:
The AttachmentsDemo example project illustrates the use of the enhanced ComboBoxAttachment and the new LabelAttachment.

### SliderMonitor
This example features a status bar at the bottom of the UI panel. The status bar shows the value of the most recently moved slider. It can also be used to change the value via text input. the text representation of the status bar is determined by the valueToText function of the parameter that is attached to the selected slider.
The example shows how a complete custom attachment can be implemented outside the framework code. 

### ConverterWithState
This example shows how a stateful text converter can be used with the AudioProcessorValueTreeState.
The radiobutton allows to change the text display of the parameter value from numbers to letters (each letter of the alphabet reperesenting a number.)
In a real world application the mechanism might be used to offer:
- different pitch display options (note number, note name, frequency, localized note names (indian, persian, italian, ...))
- different amplitude display options (percent, decibel)

### Please note:
The original AudioProcessorValueTreeState class design does not allow the addition of user defined Attachments from outside the class definition. Because of that I had to modify some files from the juce framework to make things possible.
Before compiling the demo projects you will have to replace these files with the versions that are supplied with this release:
 - juce_AudioProcessorParameters.cpp
 - juce_AudioProcessorParameterWithID.h
 - juce_AudioProcessorValueTreeState.cpp
 - juce_AudioProcessorValueTreeState.h


## Changes:
- Bugfix: AttachmentsDemo: Combobox now always shows the correct item text 
- Added a SliderMonitor example 
- Bugfix: Combobox is now initialized correctly.
- added a stateful converter example.
- added a radio button group example
- modified AudioProcessorValueTreeState to allow for separate handling of user settings
- added a canAutomate property to AudioProcessorParameterWithID

 
