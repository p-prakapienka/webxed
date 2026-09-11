#include "mapper/dx/FeedbackMapper.h"
#include "model/dx/ConversionContext.h"

#include <algorithm>
#include <cstddef>

void FeedbackMapper::map(ConversionContext& context) {
    if (context.dxFeedback <= 0) {
        context.target.setFeedback(0);
        return;
    }
    bool preserved = false;
    for (int slot = 0; slot < 4; ++slot) {
        if (context.slotToDx[static_cast<std::size_t>(slot)] == context.feedbackOp) {
            preserved = true;
            break;
        }
    }
    if (!preserved) {
        context.target.setFeedback(0);
        context.report.warnings.push_back("DX feedback lost; feedback operator was discarded");
        return;
    }
    context.target.setFeedback(std::clamp((context.dxFeedback * 127 + 3) / 7, 0, 127));
}
