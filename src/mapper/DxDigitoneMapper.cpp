#include "mapper/DxDigitoneMapper.h"
#include "model/dx/ConversionContext.h"
#include "mapper/dx/AlgorithmMapper.h"
#include "mapper/dx/EnvelopeMapper.h"
#include "mapper/dx/FeedbackMapper.h"
#include "mapper/dx/LevelMapper.h"
#include "mapper/dx/OperatorReducer.h"
#include "mapper/dx/RatioMapper.h"

#include <algorithm>
#include <string>

ConversionResult DxDigitoneMapper::convert(const DxPatch& source) {
    ConversionContext context;
    context.target = DigitonePatch();
    context.report = ConversionReport();
    context.slotToDx = {{-1, -1, -1, -1}};
    context.selected.fill(false);
    context.dnAlgorithm = 1;

    OperatorReducer operatorReducer;
    AlgorithmMapper algorithmMapper;
    RatioMapper ratioMapper;
    LevelMapper levelMapper;
    FeedbackMapper feedbackMapper;
    EnvelopeMapper envelopeMapper;

    operatorReducer.reduce(context, source);
    algorithmMapper.map(context);
    ratioMapper.map(context);
    levelMapper.map(context);
    feedbackMapper.map(context);
    envelopeMapper.map(context);
    normaliseOutput(context);

    ConversionResult result;
    result.patch = context.target;
    result.report = context.report;
    return result;
}

void DxDigitoneMapper::normaliseOutput(ConversionContext& context) {
    const std::string dxName = context.sourcePatch->getName();
    context.target.setName(dxName.empty() ? "DN" : dxName + " DN");
    context.target.setHarmonic(0.0);

    const int peak = std::max(context.target.getEnvelopeA().getLevel(), context.target.getEnvelopeB().getLevel());
    if (peak == 0) {
        context.report.warnings.push_back("Source is nearly silent; converted patch may be inaudible");
    } else if (peak < 16) {
        context.report.warnings.push_back("Converted output level is low; consider raising envelope levels");
    }
}
