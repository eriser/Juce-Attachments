/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

//==============================================================================

//==============================================================================
AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& p, UndoManager* um)
    : processor (p),
      undoManager (um),
      valueType ("PARAM"),
      valuePropertyID ("value"),
      idPropertyID ("id"),
      updatingConnections (false),
      presetID(processor.getName() + paTypePreset),
      settingsID(processor.getName() + paTypeSettings)
{
    startTimerHz (10);
    state.addListener (this);
}

AudioProcessorValueTreeState::~AudioProcessorValueTreeState() {
	saveSettings();
}

AudioProcessorParameterWithID* AudioProcessorValueTreeState::createAndAddParameter (
	const String& paramID, 
	const String& paramName,
	const String& labelText,
	NormalisableRange<float> r,
	float defaultVal,
	std::function<String(float)> valueToTextFunction,
	std::function<float(const String&)>
	textToValueFunction,
	bool canAutomate,
	bool saveStateWithPreset)
{
    // All parameters must be created before giving this manager a ValueTree state!
    jassert (! state.isValid());
   #if ! JUCE_LINUX
    jassert (MessageManager::getInstance()->isThisTheMessageThread());
   #endif

    Parameter* p = new Parameter (*this, paramID, paramName, labelText, r,
                                  defaultVal, valueToTextFunction, textToValueFunction, canAutomate, 
		                          saveStateWithPreset ? presetID : settingsID);
    processor.addParameter (p);
	paramIDs.push_back(paramID);
    return p;
}

void AudioProcessorValueTreeState::prepareToPlay() {
	for (String& paramID : paramIDs) {
		if (Parameter* p = getParameter(paramID)) {
			p->listeners.call(&AudioProcessorValueTreeState::Listener::parameterChanged, paramID, p->value);
		}
	}
}

void AudioProcessorValueTreeState::addParameterListener (StringRef paramID, Listener* listener)
{
    if (Parameter* p = Parameter::getParameterForID (processor, paramID))
        p->listeners.add (listener);
}

void AudioProcessorValueTreeState::removeParameterListener (StringRef paramID, Listener* listener)
{
    if (Parameter* p = Parameter::getParameterForID (processor, paramID))
        p->listeners.remove (listener);
}

Value AudioProcessorValueTreeState::getParameterAsValue (StringRef paramID) const
{
    if (Parameter* p = Parameter::getParameterForID (processor, paramID))
        return p->state.getPropertyAsValue (valuePropertyID, undoManager);

    return Value();
}

NormalisableRange<float> AudioProcessorValueTreeState::getParameterRange (StringRef paramID) const noexcept
{
    if (Parameter* p = Parameter::getParameterForID (processor, paramID))
        return p->range;

    return NormalisableRange<float>();
}

AudioProcessorValueTreeState::Parameter* AudioProcessorValueTreeState::getParameter (StringRef paramID) const noexcept
{
    return Parameter::getParameterForID (processor, paramID);
}

float* AudioProcessorValueTreeState::getRawParameterValue (StringRef paramID) const noexcept
{
    if (Parameter* p = Parameter::getParameterForID (processor, paramID))
        return &(p->value);

    return nullptr;
}

ValueTree AudioProcessorValueTreeState::getOrCreateChildValueTree (const String& parentID, const String& paramID)
{
	ValueTree parent(state.getOrCreateChildWithName(parentID, undoManager));
	ValueTree v(parent.getChildWithProperty(idPropertyID, paramID));

	if (!v.isValid())
	{
		v = ValueTree(valueType);
		v.setProperty(idPropertyID, paramID, undoManager);
		parent.addChild(v, -1, undoManager);
	}

	return v;
}

void AudioProcessorValueTreeState::updateParameterConnectionsToChildTrees()
{
    if (! updatingConnections)
    {
        ScopedValueSetter<bool> svs (updatingConnections, true, false);

        const int numParams = processor.getParameters().size();

        for (int i = 0; i < numParams; ++i)
        {
            AudioProcessorParameter* const ap = processor.getParameters().getUnchecked(i);
            jassert (dynamic_cast<Parameter*> (ap) != nullptr);

            Parameter* p = static_cast<Parameter*> (ap);

            p->setNewState (getOrCreateChildValueTree (p->paramType, p->paramID));
        }
    }
}

void AudioProcessorValueTreeState::valueTreePropertyChanged(ValueTree& tree, const Identifier& property)
{
//	String xmlStr = tree.toXmlString();				// Debug
//	xmlStr = tree.getParent().toXmlString();
//	xmlStr = tree.getParent().getParent().toXmlString();

	if (property == idPropertyID && tree.hasType(valueType) && tree.getParent().getParent() == state)
		updateParameterConnectionsToChildTrees();
	if (property == valuePropertyID && tree.hasType(valueType) && tree.getParent() == settings)
		saveSettings();
}

void AudioProcessorValueTreeState::valueTreeChildAdded(ValueTree& parent, ValueTree& tree)
{
//	if (parent.getParent() == state)
//		if (tree.hasType(valueType))
			updateParameterConnectionsToChildTrees();
}

void AudioProcessorValueTreeState::valueTreeChildRemoved(ValueTree& parent, ValueTree& tree, int)
{
//	if (parent.getParent() == state)
//		if (tree.hasType(valueType))
//			updateParameterConnectionsToChildTrees();
}

void AudioProcessorValueTreeState::valueTreeRedirected(ValueTree& v)
{
//	if (v == state)
		updateParameterConnectionsToChildTrees();
}

void AudioProcessorValueTreeState::valueTreeChildOrderChanged (ValueTree&, int, int) {}
void AudioProcessorValueTreeState::valueTreeParentChanged (ValueTree&) {}

void AudioProcessorValueTreeState::timerCallback()
{
    const int numParams = processor.getParameters().size();
    bool anythingUpdated = false;

    for (int i = 0; i < numParams; ++i)
    {
        AudioProcessorParameter* const ap = processor.getParameters().getUnchecked(i);
        jassert (dynamic_cast<Parameter*> (ap) != nullptr);

        Parameter* p = static_cast<Parameter*> (ap);

        if (p->needsUpdate.compareAndSetBool (0, 1))
        {
            p->copyValueToValueTree();
            anythingUpdated = true;
        }
    }

    startTimer (anythingUpdated ? 1000 / 50
                                : jlimit (50, 500, getTimerInterval() + 20));
}

AudioProcessorValueTreeState::Listener::Listener() {}
AudioProcessorValueTreeState::Listener::~Listener() {}

//==============================================================================
typedef AudioProcessorValueTreeState::Parameter Parameter;


//==============================================================================
struct AudioProcessorValueTreeState::SliderAttachment::Pimpl  : private AttachedControlBase,
                                                                private Slider::Listener
{
    Pimpl (AudioProcessorValueTreeState& s, const String& p, Slider& sl)
        : AttachedControlBase (s, p), slider (sl), ignoreCallbacks (false)
    {
        NormalisableRange<float> range (s.getParameterRange (paramID));
        slider.setRange (range.start, range.end, range.interval);
        slider.setSkewFactor (range.skew, range.symmetricSkew);

        if (AudioProcessorParameter* param = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));

        sendInitialUpdate();
        slider.addListener (this);
    }

    ~Pimpl()
    {
        slider.removeListener (this);
        removeListener();
    }

    void setValue (float newValue) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        {
            ScopedValueSetter<bool> svs (ignoreCallbacks, true);
            slider.setValue (newValue, sendNotificationSync);
        }
    }

    void sliderValueChanged (Slider* s) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        if ((! ignoreCallbacks) && (! ModifierKeys::getCurrentModifiers().isRightButtonDown()))
            setNewUnnormalisedValue ((float) s->getValue());
    }

    void sliderDragStarted (Slider*) override { beginParameterChange(); }
    void sliderDragEnded   (Slider*) override { endParameterChange();   }

    Slider& slider;
    bool ignoreCallbacks;
    CriticalSection selfCallbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

AudioProcessorValueTreeState::SliderAttachment::SliderAttachment (AudioProcessorValueTreeState& s, const String& p, Slider& sl)
    : pimpl (new Pimpl (s, p, sl))
{
}

AudioProcessorValueTreeState::SliderAttachment::~SliderAttachment() {}

//==============================================================================
struct AudioProcessorValueTreeState::ComboBoxAttachment::Pimpl  : private AttachedControlBase,
                                                                  private ComboBox::Listener
{
    Pimpl (AudioProcessorValueTreeState& s, const String& p, ComboBox& c, bool populate)
        : AttachedControlBase (s, p), combo (c), ignoreCallbacks (false)
    {
		if (populate) populateComboBox(); // the ComboBox must be populated before sendInitialUpdate is called!
        sendInitialUpdate();
        combo.addListener (this);
    }

    ~Pimpl()
    {
        combo.removeListener (this);
        removeListener();
    }

	int getItemIdFromVal(float val) { return val != 0 ? roundToInt(val) : INT_MAX; }
	float getValFromItemId(int itemId) { return itemId != INT_MAX ? (float)itemId : 0.f; }

    void setValue (float newValue) override {
		const ScopedLock selfCallbackLock (selfCallbackMutex);
        {
            ScopedValueSetter<bool> svs (ignoreCallbacks, true);
            combo.setSelectedId(getItemIdFromVal(newValue), sendNotificationSync);
        }
    }

	void comboBoxChanged(ComboBox* comboBox) override
	{
		if(comboBox != &combo) return;
		const ScopedLock selfCallbackLock(selfCallbackMutex);

		if (!ignoreCallbacks)
		{
			beginParameterChange();
			int selectedId = combo.getSelectedId();
			if (selectedId == 0) {
				if (Parameter* p = getParameter())
					setNewUnnormalisedValue( p->range.convertFrom0to1(p->getValueForText(combo.getText())));
					sendInitialUpdate(); // required to make sure the text gets set correctly when te same value gets inserted a second time in the editor.
			}
			else {
				setNewUnnormalisedValue(getValFromItemId(selectedId));
			}
			endParameterChange();
		}
	}

	void populateComboBox() {
		combo.clear();
		Parameter* p = dynamic_cast<Parameter*>(getParameter());
		if (p && p->range.interval > 0) {
			for (float itmIdx = p->range.start; itmIdx <= p->range.end; itmIdx += p->range.interval) {
				String newItem = p->getText(p->range.convertTo0to1(itmIdx), 64);
				combo.addItem(newItem, getItemIdFromVal(itmIdx));
			}
		}
	}

    ComboBox& combo;
    bool ignoreCallbacks;
    CriticalSection selfCallbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

AudioProcessorValueTreeState::ComboBoxAttachment::ComboBoxAttachment (AudioProcessorValueTreeState& s, const String& p, ComboBox& c, bool populateCombobox)
    : pimpl (new Pimpl (s, p, c, populateCombobox))
{
}

AudioProcessorValueTreeState::ComboBoxAttachment::~ComboBoxAttachment() {}

//==============================================================================
struct AudioProcessorValueTreeState::LabelAttachment::Pimpl : public AttachedControlBase,
	private Label::Listener
{
	Pimpl(AudioProcessorValueTreeState& s, const String& p, Label& lbl)
		: AttachedControlBase(s, p), label(lbl), ignoreCallbacks(false), s(s), paramID(p)
	{
		NormalisableRange<float> range(s.getParameterRange(paramID));
		sendInitialUpdate();
		label.addListener(this);
	}

	void labelTextChanged(Label* lbl) override
	{
		if (lbl != &label) return;
		const ScopedLock selfCallbackLock(selfCallbackMutex);

		if (AudioProcessorParameter* p = getParameter()) {
			float newNormalizedValue = p->getValueForText(label.getText());
			if (p->getValue() != newNormalizedValue)
				p->setValueNotifyingHost(newNormalizedValue);
			sendInitialUpdate(); // required to make sure the text gets set correctly when te same value gets inserted a second time in the editor.
		}
	}

	/** Called when a Label goes into editing mode and displays a TextEditor. */
	void editorShown(Label*, TextEditor&)  override { beginParameterChange(); }

	/** Called when a Label is about to delete its TextEditor and exit editing mode. */
	void editorHidden(Label*, TextEditor&)override { endParameterChange(); }

	~Pimpl()
	{
		label.removeListener(this);
		removeListener();
	}

	void setValue(float newValue) override
	{
		const ScopedLock selfCallbackLock(selfCallbackMutex);
		{
			ScopedValueSetter<bool> svs(ignoreCallbacks, true);
			if (Parameter* p = getParameter())
				label.setText(p->getText(p->range.convertTo0to1(newValue), 64), sendNotificationSync);
		}
	}

	bool ignoreCallbacks;
	const String& paramID;
	Label& label;
	AudioProcessorValueTreeState& s;
	CriticalSection selfCallbackMutex;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pimpl)
};

AudioProcessorValueTreeState::LabelAttachment::LabelAttachment(AudioProcessorValueTreeState& s, const String& p, Label& lbl)
	: pimpl(new Pimpl(s, p, lbl))
{
}

AudioProcessorValueTreeState::LabelAttachment::~LabelAttachment() {}

void AudioProcessorValueTreeState::LabelAttachment::replaceListener(const String& paramID) { pimpl->replaceListener(paramID); }
void AudioProcessorValueTreeState::LabelAttachment::removeListener() { pimpl->removeListener(); }
void AudioProcessorValueTreeState::LabelAttachment::sendInitialUpdate() { pimpl->sendInitialUpdate(); }
AudioProcessorValueTreeState::Parameter*  AudioProcessorValueTreeState::LabelAttachment::getParameter() { return pimpl->getParameter(); }

//==============================================================================
struct AudioProcessorValueTreeState::ButtonAttachment::Pimpl : private AttachedControlBase,
	private Button::Listener
{
	Pimpl(AudioProcessorValueTreeState& s, const String& p, Button& b)
		: AttachedControlBase(s, p), button(b), ignoreCallbacks(false)
	{
		sendInitialUpdate();
		button.addListener(this);
	}

	virtual ~Pimpl()
	{
		button.removeListener(this);
		removeListener();
	}

	void setValue(float newValue) override
	{
		const ScopedLock selfCallbackLock(selfCallbackMutex);

		{
			ScopedValueSetter<bool> svs(ignoreCallbacks, true);
			button.setToggleState(newValue >= 0.5f, sendNotificationSync);
		}
	}

	void buttonClicked(Button* b) override
	{
		const ScopedLock selfCallbackLock(selfCallbackMutex);

		if (!ignoreCallbacks)
		{
			beginParameterChange();
			setNewUnnormalisedValue(b->getToggleState() ? 1.0f : 0.0f);
			endParameterChange();
		}
	}

	Button& button;
	bool ignoreCallbacks;
	CriticalSection selfCallbackMutex;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pimpl)
};

AudioProcessorValueTreeState::ButtonAttachment::ButtonAttachment(AudioProcessorValueTreeState& s, const String& p, Button& b)
	: pimpl(new Pimpl(s, p, b))
{
}

AudioProcessorValueTreeState::ButtonAttachment::~ButtonAttachment() {}

