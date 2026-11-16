// Draws the plugin window to a PNG (developer tool; see CMakeLists.txt).
#include "PluginProcessor.h"

#include <iostream>
#include <memory>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: render_ui OUT.png [maqam_id] [variant_id]\n";
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if (auto* maqamProcessor = dynamic_cast<MaqamProcessor*>(processor.get())) {
        if (argc > 2)
            maqamProcessor->setMaqam(argv[2]);
        if (argc > 3)
            maqamProcessor->setVariant(argv[3]);
    }
    {
        // The processor owns the active editor and deletes it through editorBeingDeleted.
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorIfNeeded());
        const juce::Image image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
        const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        out.deleteFile();
        juce::FileOutputStream stream(out);
        const bool ok = stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream);
        processor->editorBeingDeleted(editor.get());
        if (!ok) {
            std::cerr << "could not write " << out.getFullPathName() << "\n";
            return 1;
        }
    }
    return 0;
}
