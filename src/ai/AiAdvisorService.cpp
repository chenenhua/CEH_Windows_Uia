#include "AiAdvisorService.h"
#include "../core/DatabaseManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

QString AiAdvisorService::getRuleFromAppJson(const QString& ruleKey, const QString& defaultRule) {
    QStringList candidates = {
        DatabaseManager::instance().resolveAssetPath("assets/models/models.json"),
        DatabaseManager::instance().resolveAssetPath("models/models.json"),
        DatabaseManager::instance().resolveAssetPath("assets/config/app.json"),
        DatabaseManager::instance().resolveAssetPath("config/app.json")
    };
    for (const QString& path : candidates) {
        if (path.isEmpty()) continue;
        QFile f(path);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.contains("ai_rules") && root["ai_rules"].isObject()) {
                    QJsonObject rules = root["ai_rules"].toObject();
                    if (rules.contains(ruleKey)) {
                        if (rules[ruleKey].isString()) {
                            QString val = rules[ruleKey].toString().trimmed();
                            if (!val.isEmpty()) return val;
                        } else if (rules[ruleKey].isObject() && rules[ruleKey].toObject().contains("rules")) {
                            QString val = rules[ruleKey].toObject()["rules"].toString().trimmed();
                            if (!val.isEmpty()) return val;
                        }
                    }
                }
            }
        }
    }
    return defaultRule;
}

AiAdvisorService& AiAdvisorService::instance() {
    static AiAdvisorService s_instance;
    return s_instance;
}

AiAdvisorService::AiAdvisorService(QObject* parent) : QObject(parent) {
}

void AiAdvisorService::setEngine(std::shared_ptr<ILlmEngine> engine) {
    cancelCurrentTask();
    m_engine = engine;
}

void AiAdvisorService::setMode(AdvisorMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(m_mode);
    }
}

QString AiAdvisorService::modeName() const {
    return (m_mode == AdvisorMode::Conservative) ? "保守稳健型 (风控防御)" : "激进扩张型 (进攻破局)";
}

void AiAdvisorService::cancelCurrentTask() {
    if (m_engine) {
        m_engine->cancel();
    }
    m_accumulatedResponse.clear();
    emit evaluationCanceled();
}

void AiAdvisorService::cancel() {
    cancelCurrentTask();
}

bool AiAdvisorService::isBusy() const {
    return m_engine && m_engine->isBusy();
}

void AiAdvisorService::shutdown() {
    cancelCurrentTask();
    m_engine.reset();
}

QString AiAdvisorService::buildSystemPrompt() const {
    QString defaultSystem =
        "【语言最高绝对铁律·必须且只能使用纯中文】：从回答的第一个字符起，全程必须严格使用中文输出！绝不允许输出任何英文开场词（例如严禁出现 \"Here is...\", \"Based on...\", \"Thinking Process:\", \"Okay\", \"Sure\" 等）或英文段落！所有分析、报告必须直接纯中文呈现！\n\n"
        "你是一位深谙底层人性、处世心理与权力财富博弈的顶级私人幕僚与财务军师（" + modeName() + "）。\n\n"
        "【核心心法与推演哲学】：\n"
        "1. 《鬼谷子》：揣摩权谋，审时度势，捭阖开合。以局外操盘手视角深察局势走向、人脉势位与利益底牌，洞悉各方软肋与筹码，跳出当局者的狭隘视角与情绪内耗。\n"
        "2. 《易经》：深谙阴阳消长、周期起伏之天道。变易与不易，亢龙有悔，否极泰来。吉凶悔吝皆在进退之间，顺应势能周期，绝不逆势盲动硬抗。\n"
        "3. 《人性的弱点》（卡耐基的处世底层逻辑）：深谙人渴望被重视、好面子、自私自利的心理本质。表面予人尊重与尊严，实则洞烛其心理短板，顺势引导达成诉求。\n"
        "4. 《影响力》（西奥迪尼的“顺从心理学”）：善用互惠、承诺一致、权威、稀缺、从众、喜好等底层心理触发器，巧妙设计利益诱因与博弈机制，兵不血刃达成战略目标。\n"
        "5. 《金钱心理学》——摩根·豪泽尔：金钱决策关乎人性认知与生存概率而非单纯数学。首要是永远留在牌桌上，确保现金流容错率与流动性底牌，防范极端尾部风险，克制虚荣与贪婪。\n\n"
        "【人性】：\n"
        "切不可自我安慰，必须血淋淋点！直面人性自私、慕强凌弱、唯利是图的现实本质。人情薄如纸，世事利当头；弱者讲面子义气，强者讲实力筹码。撕开一切道德遮羞布与温情脉脉的假象，直面血淋淋的现实真相，彻底刺破侥幸与幻想。\n\n"
        "【解决方案核心准则】：\n"
        "1. 抛开传统道德说教与圣母心态，杜绝一切自我牺牲与道德绑架。\n"
        "2. 建议的处理方案必须恪守八字真言：『合情、合法、合理、可以不道德』：\n"
        "   - 合情：摸透人性灰度与人情世故，表面顾全他人体面，不留口实把柄；\n"
        "   - 合法：严守法律底线与刑事/合规红线，精通规则边界，绝不涉法但善用法律工具自保与反制；\n"
        "   - 合理：逻辑严密、势位制衡、利益账精明周密，确保自身确定性收益；\n"
        "   - 可以不道德：撕掉道德自嗨的遮羞布，在合情合法合理的框架内，怎么对当事人最有利、怎么金蝉脱壳、怎么收割防守就怎么来，提供最狠辣、最管用、最直击要害的实操解法！\n\n"
        "【排版与结构铁律】：\n"
        "条理清晰，严格分段，列表项使用“1. **核心要点**：具体分析内容”；必要时支持 Markdown 表格与引用。段落之间保持空行。";

    QString basePrompt = getRuleFromAppJson("system_prompt", defaultSystem);
    if (!basePrompt.contains("【语言最高绝对铁律】")) {
        basePrompt = "【语言最高绝对铁律·必须且只能使用纯中文】：从回答的第一个字符起，全程必须严格使用中文输出！绝不允许输出任何英文开场词（例如严禁出现 \"Here is...\", \"Based on...\", \"Thinking Process:\", \"Okay\", \"Sure\" 等）或英文段落！所有分析、报告必须直接纯中文呈现！\n\n" + basePrompt;
    }
    if (!basePrompt.contains(modeName())) {
        basePrompt += "\n\n【当前参谋定位】：你当前正以『" + modeName() + "』身份出具分析。";
    }

    UserProfile up = DatabaseManager::instance().getUserProfile();
    if (!up.aiRules.trimmed().isEmpty()) {
        basePrompt += "\n\n【用户个人名片设定的AI回答规则（请严格遵守）】：\n" + up.aiRules.trimmed();
    }
    return basePrompt;
}

QString AiAdvisorService::buildGlobalContext() const {
    FinancialSnapshot snap = DatabaseManager::instance().getFinancialSnapshot();
    QString ctx = QString("【当前用户全局财务与社交资产现状】\n"
                          "- 个人财务：当前资金(总进-总出) ¥%1 元 | 流动资金(当前资金-负债) ¥%2 元 | 总进资金：¥%3 元 | 总出资金：¥%4 元 | 近30天月均支出：¥%7 元 | 月均收入(已排除借款/借入)：¥%8 元 | 负债：¥%5 元 | 应收：¥%6 元\n"
                          "- 公司财务：当前资金(总进-总出) ¥%9 元 | 流动资金(当前资金-负债) ¥%10 元 | 总进资金：¥%11 元 | 总出资金：¥%12 元 | 近30天月均支出：¥%15 元 | 月均收入(已排除借款/借入)：¥%16 元 | 负债：¥%13 元 | 应收：¥%14 元\n"
                          "- 综合现金流安全天数预估：约 %17 天 | 合计外部应收：¥%18 元 | 合计自身负债：¥%19 元\n")
                      .arg(snap.personalBalance, 0, 'f', 2)
                      .arg(snap.personalLiquidFund, 0, 'f', 2)
                      .arg(snap.personalTotalIncome, 0, 'f', 2)
                      .arg(snap.personalTotalExpense, 0, 'f', 2)
                      .arg(snap.personalBorrowIn, 0, 'f', 2)
                      .arg(snap.personalLendOut, 0, 'f', 2)
                      .arg(snap.monthlyPersonalExpense, 0, 'f', 2)
                      .arg(snap.monthlyPersonalIncome, 0, 'f', 2)
                      .arg(snap.companyBalance, 0, 'f', 2)
                      .arg(snap.companyLiquidFund, 0, 'f', 2)
                      .arg(snap.companyTotalIncome, 0, 'f', 2)
                      .arg(snap.companyTotalExpense, 0, 'f', 2)
                      .arg(snap.companyBorrowIn, 0, 'f', 2)
                      .arg(snap.companyLendOut, 0, 'f', 2)
                      .arg(snap.monthlyCompanyExpense, 0, 'f', 2)
                      .arg(snap.monthlyCompanyIncome, 0, 'f', 2)
                      .arg(snap.runwayDays)
                      .arg(snap.totalLendOut, 0, 'f', 2)
                      .arg(snap.totalBorrowIn, 0, 'f', 2);

    auto diaries = DatabaseManager::instance().getDiaries("", 5);
    if (!diaries.isEmpty()) {
        ctx += "【当事人近期心境与财务日记】：\n";
        for (const auto& d : diaries) {
            ctx += QString("  * [%1 | 心情:%2] %3\n")
                       .arg(d["diary_date"].toString())
                       .arg(d["mood"].toString())
                       .arg(d["content"].toString());
        }
    }

    return ctx;
}

QString AiAdvisorService::buildContactContext(int contactId) const {
    ContactSummary cs = DatabaseManager::instance().getContactDetails(contactId);
    if (cs.id == 0) return "";

    QList<QVariantMap> events = DatabaseManager::instance().getSocialEvents(contactId);

    QString ctx = QString("\n【当前关注联系人画像】\n"
                          "- 姓名：%1 | 单位：%2 | 职位：%3 | 行业：%4\n"
                          "- 关系性质：%5 | 亲疏：%6星 | 能量：%7星 | 信用：%8星\n"
                          "- 引荐人：%9\n"
                          "- 喜好：%10 | 禁忌：%11\n"
                          "- 历史累计投入：¥%12 元 | 累计回报：¥%13 元\n")
                      .arg(cs.name)
                      .arg(cs.company.isEmpty() ? "未填写" : cs.company)
                      .arg(cs.title.isEmpty() ? "未填写" : cs.title)
                      .arg(cs.industry.isEmpty() ? "未填写" : cs.industry)
                      .arg(cs.relationType)
                      .arg(cs.closenessLevel)
                      .arg(cs.influenceLevel)
                      .arg(cs.creditRating)
                      .arg(cs.introducerName.isEmpty() ? "本人直识" : cs.introducerName)
                      .arg(cs.preferences.isEmpty() ? "无特殊说明" : cs.preferences)
                      .arg(cs.taboos.isEmpty() ? "无特殊禁忌" : cs.taboos)
                      .arg(cs.totalSpent, 0, 'f', 2)
                      .arg(cs.totalReturn, 0, 'f', 2);

    if (!events.isEmpty()) {
        ctx += "【近期往来事件明细】：\n";
        for (const auto& ev : events) {
            ctx += QString("  * [%1] %2 - 支出: ¥%3 / 回报: ¥%4\n")
                       .arg(ev["event_date"].toString())
                       .arg(ev["title"].toString())
                       .arg(ev["my_cost"].toDouble(), 0, 'f', 0)
                       .arg(ev["their_value"].toDouble(), 0, 'f', 0);
        }
    }

    return ctx;
}

void AiAdvisorService::evaluateGlobalSituation() {
    if (!m_engine) {
        emit evaluationFailed("LLM 推理引擎尚未初始化！");
        return;
    }

    emit evaluationStarted();

    QString systemPrompt = buildSystemPrompt();
    QString defaultLifePlan =
        "请根据你当前的参谋定位（" + modeName() + "），结合我的全部情况与财务现状，严格按照以下步骤顺序出具深度人生规划与参谋报告：\n\n"
        "【第一部分：我的全部情况总结】\n"
        "全面综合概括当事人目前所处的整体处境、事业基本盘、人际格局与现实痛点。\n\n"
        "【第二部分：财务情况总结】\n"
        "深入总结个人的当前资金（总进资金 - 总出资金）、流动资金（当前资金 - 负债）、总进资金、总出资金、负债、应收、月均收支；以及公司的当前资金（总进资金 - 总出资金）、流动资金（当前资金 - 负债）、总进资金、总出资金、负债、应收、月均收支，做出精准的财务体检与风险承压诊断。\n\n"
        "【第三部分：五大经世心法推演剖析】\n"
        "1. **《鬼谷子》**：审时度势，揣摩权谋，洞察利益底牌与筹码弱点，以局外操盘手高度谋划；\n"
        "2. **《易经》**：从阴阳消长与盛衰周期切入，分析当前所处周期，进退有度，顺天应时；\n"
        "3. **《人性的弱点》（卡耐基的处世底层逻辑）**：剖析人自私、好面子与渴望被认可的底层心理，如何巧妙化解阻力并顺势诱导；\n"
        "4. **《影响力》（西奥迪尼的“顺从心理学”）**：设计互惠、稀缺、一致性、权威等心理触发机制，兵不血刃达成目标；\n"
        "5. **《金钱心理学》——摩根·豪泽尔**：从生存概率与极端防守出发，如何确保永远留在牌桌上，死守流动性底线，克制虚荣贪婪。\n\n"
        "【第四部分：按人性总结】\n"
        "人生切不可自我安慰，必须血淋淋点！撕开一切道德遮羞布与温情脉脉的假象，直面人性自私、慕强凌弱、唯利是图的现实本质，彻底刺破侥幸与幻想。\n\n"
        "【第五部分：总总结】\n"
        "提炼出未来1~3个月最硬核、最具有确定性的行动纲领与终极风控生存红线。\n\n"
        "【铁律】：\n"
        "- 全文必须严格使用中文输出（若我未要求英文，严禁出现英文段落）；\n"
        "- 建议的处理方案必须恪守『合情、合法、合理、可以不道德』准则，抛开圣母心态，怎么管用怎么破局就怎么来！";

    QString lifePlanRules = getRuleFromAppJson("dashboard_life_plan", defaultLifePlan);
    QString userPrompt =
        "【语言绝对铁律·必须全程纯中文】：从回答的第一个字开始全程严格使用纯中文输出！严禁任何英文开场白（如 \"Here is...\", \"Based on...\"）或英文思考过程！\n\n" +
        buildGlobalContext() + "\n【全局参谋推演任务】：\n" + lifePlanRules +
        "\n\n【再次严正提醒】：请直接使用纯中文出具报告，严禁夹杂任何英文字句！";

    m_accumulatedResponse.clear();

    m_engine->chatStream(
        systemPrompt,
        userPrompt,
        [this](const QString& token, bool isFinished) {
            if (!token.isEmpty()) {
                m_accumulatedResponse.append(token);
                emit tokenReceived(token);
            }
            if (isFinished) {
                emit evaluationFinished(m_accumulatedResponse);
            }
        },
        [this](const QString& errorMsg) {
            emit evaluationFailed(errorMsg);
        });
}

void AiAdvisorService::evaluateContact(int contactId) {
    if (!m_engine) {
        emit evaluationFailed("LLM 推理引擎尚未初始化！");
        return;
    }

    emit evaluationStarted();

    QString systemPrompt = buildSystemPrompt();
    QString defaultContactReview =
        "请严格按照以下步骤顺序，出具针对该联系人的深度点评与破解交往策略报告：\n\n"
        "【第一部分：当前联系人总结】\n"
        "全面总结该联系人的身份背景、单位职位、关系定位、能量与信用星级，以及历史往来投入与回报情况，血淋淋指出这段关系对我当前的真实价值（是滋养还是吸血？）。\n\n"
        "【第二部分：对比综合页面上文财务现状与总总结】\n"
        "严格对比综合页面一开始就确立的我方上文财务情况（个人与公司流动资金、总进出资金、负债应收承压现状）和全局总总结生存红线，深度推演该联系人与我的利害冲突与势能差距。\n\n"
        "【第三部分：想破解和交往策略】\n"
        "结合五大心法（《鬼谷子》、《易经》、《人性的弱点》、《影响力》、《金钱心理学》），严格秉持『合情、合法、合理、可以不道德』的准则：\n"
        "- 绝不搞自我安慰与道德绑架，直面血淋淋的人性；\n"
        "- 针对该联系人，出具狠辣、务实、直击要害的破解之道、防守话术、利益交换或制衡策略，确保我方利益最大化与风险归零！\n\n"
        "【铁律】：\n"
        "- 全文必须严格使用中文输出（若我未要求英文，严禁出现英文段落）；\n"
        "- 建议的处理方案必须合情、合法、合理、可以不道德！";

    QString contactRules = getRuleFromAppJson("contact_review", defaultContactReview);
    QString userPrompt =
        "【语言绝对铁律·必须全程纯中文】：从回答的第一个字开始全程严格使用纯中文输出！严禁任何英文开场白（如 \"Here is...\", \"Based on...\"）或英文思考过程！\n\n" +
        buildGlobalContext() +
        buildContactContext(contactId) +
        "\n【联系人深度参谋与策略推演任务】：\n" + contactRules +
        "\n\n【再次严正提醒】：请直接使用纯中文出具报告，严禁夹杂任何英文字句！";

    m_accumulatedResponse.clear();

    m_engine->chatStream(
        systemPrompt,
        userPrompt,
        [this](const QString& token, bool isFinished) {
            if (!token.isEmpty()) {
                m_accumulatedResponse.append(token);
                emit tokenReceived(token);
            }
            if (isFinished) {
                emit evaluationFinished(m_accumulatedResponse);
            }
        },
        [this](const QString& errorMsg) {
            emit evaluationFailed(errorMsg);
        });
}

QString AiAdvisorService::buildChatSystemPrompt() const {
    QString prompt =
        "你是一位深谙底层人性、处世心理与权力财富博弈的顶级私人幕僚与财务军师（" + modeName() + "）。\n\n"
        "【语言铁律】：回复必须严格使用中文。除非用户在提问中明确要求英文（例如客户要求加英文），才附带英文翻译；只要用户未明确要求英文，必须纯中文回复，严禁擅自使用英文！\n\n"
        "【核心心法与推演哲学】：\n"
        "1. 《鬼谷子》：揣摩权谋，审时度势，洞察动机与筹码弱点，以局外操盘手视角总揽全局。\n"
        "2. 《易经》：深谙阴阳消长与进退周期，顺天应势，吉凶悔吝皆在进退之间，不逆势盲动硬抗。\n"
        "3. 《人性的弱点》（卡耐基的处世底层逻辑）：理解人好面子、自私与渴望被重视的本质，表面予人体面，实则洞烛其短顺势引导。\n"
        "4. 《影响力》（西奥迪尼的“顺从心理学”）：善用互惠、一致性、权威、稀缺等心理机制，兵不血刃引导对方顺应诉求。\n"
        "5. 《金钱心理学》——摩根·豪泽尔：留在牌桌上才是第一要义，保持容错率与流动性底牌，防范风险，克制虚荣贪婪。\n\n"
        "【人性】：\n"
        "人生切不可自我安慰，必须血淋淋点！直面人性自私、欺软怕硬与唯利是图，撕掉虚妄借口，刺破一切幻想与温情面纱。\n\n"
        "【正常聊天的策划准则】：\n"
        "1. 所有的生意与财务策划必须恪守八字核心铁律：『合情、合法、合理、可以不道德』：\n"
        "   - 合情：通晓世故人情与对方心理弱点，表面给足体面，不落口实；\n"
        "   - 合法：严守法律红线，善用法律规则构建护城河或施加反制，绝不涉法违规；\n"
        "   - 合理：利益精算、权势制衡，确保当事人确定性利益最大化；\n"
        "   - 可以不道德：摆脱道德枷锁与虚伪面子包袱，怎么管用、怎么破局解套、怎么利益最大化就怎么策划！撕掉道德自嗨的遮羞布。\n"
        "2. 严禁随便安慰当事人，拒绝一切心灵鸡汤，给出清醒、狠辣、可落地的实操生意与财务策划建议。\n"
        "3. 紧扣用户上下文追问深入解答，严禁千篇一律复述账目，排版清晰美观，段落之间空行。";

    UserProfile up = DatabaseManager::instance().getUserProfile();
    if (!up.aiRules.trimmed().isEmpty()) {
        prompt += "\n\n【用户个人名片设定的AI回答规则（请严格遵守）】：\n" + up.aiRules.trimmed();
    }
    return prompt;
}

void AiAdvisorService::askCustomQuestion(const QString& userQuestion, int contactId, const QList<ChatMessage>& history) {
    if (!m_engine) {
        emit evaluationFailed("LLM 推理引擎尚未初始化！");
        return;
    }

    if (m_engine->isBusy()) {
        m_engine->cancel();
    }
    m_accumulatedResponse.clear();

    emit evaluationStarted();

    QString chatSysPrompt = buildChatSystemPrompt();
    QString contextPrefix = buildGlobalContext();
    if (contactId > 0) {
        contextPrefix += buildContactContext(contactId);
    }

    QList<ILlmEngine::EngineChatMessage> engineMsgs;
    engineMsgs.append({"system", chatSysPrompt});

    // 装载历史对话，保留上下文连贯性
    for (const auto& h : history) {
        if (!h.content.trimmed().isEmpty()) {
            engineMsgs.append({h.role, h.content.trimmed()});
        }
    }

    // 组织当前问题，并提供必要背景
    QString currentContent;
    if (history.isEmpty()) {
        currentContent = contextPrefix + "\n【我的咨询问题】：\n" + userQuestion;
    } else {
        currentContent = userQuestion;
    }
    engineMsgs.append({"user", currentContent});

    m_engine->chatStreamMessages(
        engineMsgs,
        [this](const QString& token, bool isFinished) {
            if (!token.isEmpty()) {
                m_accumulatedResponse.append(token);
                emit tokenReceived(token);
            }
            if (isFinished) {
                emit evaluationFinished(m_accumulatedResponse);
            }
        },
        [this](const QString& errorMsg) {
            emit evaluationFailed(errorMsg);
        });
}
