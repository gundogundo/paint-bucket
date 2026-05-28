#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <vector>
#include <cmath>

using namespace geode::prelude;

// 모드 전역 변수 설정
bool g_paintMode = false;
cocos2d::ccColor3B g_paintColor = {255, 255, 255};
float g_precision = 0.5f;

class $modify(MyEditorUI, EditorUI) {
    struct Fields {
        std::vector<GameObject*> nearbyObjects;
        CCNode* paintTabContent = nullptr;
        bool paintTabOpen = false;
        CCTextInputNode* colorInput = nullptr;
        std::vector<std::vector<GameObject*>> undoStack;
        std::vector<std::vector<GameObject*>> redoStack;
        CCLabelBMFont* precisionLabel = nullptr;
    };

    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;

        // 상단 빌드 탭 메뉴에 페인트 버킷(망치 아이콘) 버튼
        if (auto tabMenu = this->getChildByID("build-tabs-menu")) {
            auto tabSprite = CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png");
            if (!tabSprite) tabSprite = CCSprite::create(); // 리소스 로드 실패 시 크래시 방지 쉴드
            tabSprite->setScale(0.8f);

            auto tabBtn = CCMenuItemSpriteExtra::create(tabSprite, this, menu_selector(MyEditorUI::onPaintTabClicked));
            tabBtn->setID("paint-tab-button"_spr);
            tabMenu->addChild(tabBtn);
            tabMenu->updateLayout();
        }

        this->createPaintTabContent();
        return true;
    }

    // 일시정지 창 열릴 때 예기치 못한 매핑 버그 방지를 위해 모드 강제 오프
    void onPause(CCObject* sender) {
        g_paintMode = false;
        EditorUI::onPause(sender);
    }

    // 페인트 툴 조작 패널(UI) 생성 및 배치
    void createPaintTabContent() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto panel = CCNode::create();
        panel->setID("paint-tab-content"_spr);
        panel->setVisible(false);

        CCPoint menuPos = {winSize.width / 2, 570};
        if (auto tabMenu = this->getChildByID("build-tabs-menu")) {
            menuPos = tabMenu->getPosition();
            menuPos.y += 130;
        }

        // UI 배경상자 셋업
        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({550, 160});
        bg->setPosition(menuPos);
        bg->setOpacity(220);
        panel->addChild(bg);

        // 1행: Color 입력 패널
        auto label = CCLabelBMFont::create("Color:", "bigFont.fnt");
        label->setScale(0.45f);
        label->setPosition({menuPos.x - 200, menuPos.y + 45});
        panel->addChild(label);

        auto inputBg = CCScale9Sprite::create("square02_001.png");
        inputBg->setContentSize({220, 36});
        inputBg->setPosition({menuPos.x + 30, menuPos.y + 45});
        inputBg->setOpacity(100);
        panel->addChild(inputBg);

        // CCTextInputNode의 텍스트가 거대해지는 것을 막기 위한 필수 속성들 지정
        auto input = CCTextInputNode::create(210, 30, " ", "bigFont.fnt");
        input->setString("#FFFFFF");
        input->setMaxLabelScale(0.5f); // 글자 최대 크기 제한 고정
        input->setLabelPlaceholderScale(0.0f);
        input->setAllowedChars("#0123456789ABCDEFabcdef");
        input->setPosition({menuPos.x + 30, menuPos.y + 45});
        input->setID("color-input"_spr);
        panel->addChild(input);
        m_fields->colorInput = input;

        // 2행: 정밀도 설정
        auto precLabel = CCLabelBMFont::create("Precision:", "bigFont.fnt");
        precLabel->setScale(0.45f);
        precLabel->setPosition({menuPos.x - 120, menuPos.y - 10});
        panel->addChild(precLabel);

        auto valueLabel = CCLabelBMFont::create("0.50", "bigFont.fnt");
        valueLabel->setScale(0.45f);
        valueLabel->setID("precision-value"_spr);
        valueLabel->setPosition({menuPos.x + 40, menuPos.y - 10});
        panel->addChild(valueLabel);
        m_fields->precisionLabel = valueLabel;

        // 3행: 버튼 기능 메뉴 구성
        auto menu = CCMenu::create();
        menu->setPosition(menuPos);
        menu->setTouchPriority(-200);

        // 정밀도 감소 버튼 (< 화살표)
        auto minusSprite = CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png");
        if (!minusSprite) minusSprite = CCSprite::create();
        minusSprite->setScale(0.75f);
        auto minusBtn = CCMenuItemSpriteExtra::create(minusSprite, this, menu_selector(MyEditorUI::onPrecisionMinus));
        minusBtn->setPosition({-10, -10});

        // 정밀도 증가 버튼 (> 화살표)
        auto plusSprite = CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png");
        if (!plusSprite) plusSprite = CCSprite::create();
        plusSprite->setScale(0.75f);
        auto plusBtn = CCMenuItemSpriteExtra::create(plusSprite, this, menu_selector(MyEditorUI::onPrecisionPlus));
        plusBtn->setPosition({90, -10});

        // 색상 적용 버튼 - [버그 수정] 스케일을 버튼이 아닌 라벨에 설정
        auto applyLabel = CCLabelBMFont::create("Apply", "bigFont.fnt");
        applyLabel->setScale(0.45f);
        auto applyBtn = CCMenuItemSpriteExtra::create(applyLabel, this, menu_selector(MyEditorUI::onApplyColor));
        applyBtn->setPosition({-150, -60});

        // 페인트 모드 활성화 버튼
        auto paintSprite = CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png");
        if (!paintSprite) paintSprite = CCSprite::create();
        paintSprite->setScale(0.8f);
        auto paintBtn = CCMenuItemSpriteExtra::create(paintSprite, this, menu_selector(MyEditorUI::onPaintBucketClicked));
        paintBtn->setPosition({-50, -60});

        // 실행 취소 버튼 - [버그 수정] 스케일을 버튼이 아닌 라벨에 설정
        auto undoLabel = CCLabelBMFont::create("Undo", "bigFont.fnt");
        undoLabel->setScale(0.45f);
        auto undoBtn = CCMenuItemSpriteExtra::create(undoLabel, this, menu_selector(MyEditorUI::onPaintUndo));
        undoBtn->setPosition({50, -60});

        // 다시 실행 버튼 - [버그 수정] 스케일을 버튼이 아닌 라벨에 설정
        auto redoLabel = CCLabelBMFont::create("Redo", "bigFont.fnt");
        redoLabel->setScale(0.45f);
        auto redoBtn = CCMenuItemSpriteExtra::create(redoLabel, this, menu_selector(MyEditorUI::onPaintRedo));
        redoBtn->setPosition({150, -60});

        menu->addChild(minusBtn); menu->addChild(plusBtn); menu->addChild(applyBtn);
        menu->addChild(paintBtn); menu->addChild(undoBtn); menu->addChild(redoBtn);
        panel->addChild(menu);

        this->addChild(panel, 100);
        m_fields->paintTabContent = panel;
    }

    // Precision 텍스트 갱신 헬퍼
    void updatePrecisionLabel() {
        if (!m_fields->precisionLabel) return;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", g_precision);
        m_fields->precisionLabel->setString(buf);
    }

    void onPrecisionMinus(CCObject*) {
        g_precision = std::max(0.05f, std::round((g_precision - 0.05f) * 100.0f) / 100.0f);
        updatePrecisionLabel();
    }

    void onPrecisionPlus(CCObject*) {
        g_precision = std::min(0.5f, std::round((g_precision + 0.05f) * 100.0f) / 100.0f);
        updatePrecisionLabel();
    }

    // 받은 색깔 값 cocos2d로 변환
    bool parseHexColor(const std::string& hex, cocos2d::ccColor3B& out) {
        std::string h = (!hex.empty() && hex[0] == '#') ? hex.substr(1) : hex;
        if (h.size() != 6) return false;
        try {
            out = {
                (GLubyte)std::stoi(h.substr(0, 2), nullptr, 16),
                (GLubyte)std::stoi(h.substr(2, 2), nullptr, 16),
                (GLubyte)std::stoi(h.substr(4, 2), nullptr, 16)
            };
            return true;
        } catch (...) { return false; }
    }

    void onApplyColor(CCObject*) {
        if (!m_fields->colorInput) return;
        cocos2d::ccColor3B color;
        if (parseHexColor(m_fields->colorInput->getString(), color)) {
            g_paintColor = color;
            Notification::create("Color applied!", NotificationIcon::Success)->show();
        } else {
            Notification::create("Invalid! Use #RRGGBB", NotificationIcon::Error)->show();
        }
    }

    void onPaintTabClicked(CCObject*) {
        m_fields->paintTabOpen = !m_fields->paintTabOpen;
        if (m_fields->paintTabContent)
            m_fields->paintTabContent->setVisible(m_fields->paintTabOpen);
    }

    void onPaintBucketClicked(CCObject*) {
        g_paintMode = !g_paintMode;
        Notification::create(std::string("Paint Bucket ") + (g_paintMode ? "ON" : "OFF"), NotificationIcon::Info)->show();
    }

    // 뒤로 가기
    void onPaintUndo(CCObject*) {
        if (m_fields->undoStack.empty()) return;
        auto layer = this->m_editorLayer;
        auto lastBatch = m_fields->undoStack.back();
        m_fields->undoStack.pop_back();
        for (auto* obj : lastBatch) {
            if (obj) {
                obj->setVisible(false);
                obj->m_isDisabled = true;
                layer->m_objects->removeObject(obj);
            }
        }
        m_fields->redoStack.push_back(lastBatch);
        layer->updateOptions();
    }

    // 앞으로 가기
    void onPaintRedo(CCObject*) {
        if (m_fields->redoStack.empty()) return;
        auto layer = this->m_editorLayer;
        auto lastBatch = m_fields->redoStack.back();
        m_fields->redoStack.pop_back();
        for (auto* obj : lastBatch) {
            if (obj) {
                obj->setVisible(true);
                obj->m_isDisabled = false;
                layer->m_objects->addObject(obj);
            }
        }
        m_fields->undoStack.push_back(lastBatch);
        layer->updateOptions();
    }

    // 과부하 방지: 터치 지점 주변(1500유닛) 오브젝트만 확인
    void updateNearbyObjects(CCPoint center, float range) {
        m_fields->nearbyObjects.clear();
        auto objects = this->m_editorLayer->m_objects;
        if (!objects) return;
        CCRect searchArea = { center.x - range, center.y - range, range * 2, range * 2 };
        for (auto* obj : CCArrayExt<GameObject*>(objects)) {
            if (searchArea.intersectsRect(obj->boundingBox()))
                m_fields->nearbyObjects.push_back(obj);
        }
    }

    // 오브젝트의 회전 각도까지 고려해 충돌 판정
    bool isInside(GameObject* obj, CCPoint pos) {
        if (!obj->boundingBox().containsPoint(pos)) return false;
        float angle = obj->getRotation();
        CCPoint localPos = pos - obj->getPosition();
        float rad = CC_DEGREES_TO_RADIANS(angle);
        float lx = localPos.x * std::cos(rad) - localPos.y * std::sin(rad);
        float ly = localPos.x * std::sin(rad) + localPos.y * std::cos(rad);
        return std::abs(lx) <= (obj->getContentSize().width * std::abs(obj->getScaleX()) / 2.0f) &&
               std::abs(ly) <= (obj->getContentSize().height * std::abs(obj->getScaleY()) / 2.0f);
    }

    // 클릭한 위치에 블록이 있는지 확인
    bool isWallAt(CCPoint pos) {
        for (auto* obj : m_fields->nearbyObjects)
            if (isInside(obj, pos)) return true;
        return false;
    }

    // 정한 정밀도를 기반으로 좌/우 벽 모서리 경계를 스캔
    float findEdge(CCPoint start, float dir, float maxRange) {
        float x = start.x;
        for (float dist = 0.0f; dist < maxRange; dist += g_precision) {
            if (isWallAt({x + dir * g_precision, start.y})) break;
            x += dir * g_precision;
        }
        return x;
    }

    // 안 쓰는 컬러 채널을 새로 만들기
    int registerColorChannel(cocos2d::ccColor3B color) {
        auto levelSettings = this->m_editorLayer->m_levelSettings;
        if (!levelSettings || !levelSettings->m_effectManager) return -1;
        auto em = levelSettings->m_effectManager;

        int channelID = std::max(1, levelSettings->m_nextFreeID);
        levelSettings->m_nextFreeID = channelID + 1;

        auto ca = ColorAction::create(color, color, 0.0f, 0.0, false, 0, 1.0f, 1.0f);
        if (!ca) return -1;

        ca->m_color = ca->m_fromColor = ca->m_toColor = color;
        ca->m_currentOpacity = ca->m_fromOpacity = ca->m_toOpacity = 1.0f;
        ca->m_stepFinished = true;

        em->setColorAction(ca, channelID);
        em->calculateBaseActiveColors();
        em->processColors();
        return channelID;
    }

    // 채널 ID와 색상을 오브젝트에 강제 주입
    void applyColorToObject(GameObject* obj, int channelID) {
        if (channelID < 0) return;
        if (obj->m_baseColor) {
            obj->m_baseColor->m_colorID = channelID;
            obj->m_baseColor->m_opacity = 1.0f;
        } else {
            obj->setColor(g_paintColor);
        }
        if (obj->m_detailColor) obj->m_detailColor->m_colorID = channelID;

        // [안전 복구] Geode 구버전 바인딩 빌드 호환성을 위해 수동 채널 업데이트 호출
        obj->updateCustomColorType(1);
        obj->updateCustomColorType(2);
    }

    // 화면 클릭 감지 및 페인트 기능 실행
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        // 모드 탭이 열려있을 때는 모드 패널 내부의 클릭 입력을 가로채서 맵 오브젝트 수정을 방지
        if (m_fields->paintTabOpen && m_fields->paintTabContent) {
            auto touchPos = touch->getLocation();
            for (auto* child : CCArrayExt<CCNode*>(m_fields->paintTabContent->getChildren())) {
                if (child && child->boundingBox().containsPoint(m_fields->paintTabContent->convertToNodeSpace(touchPos)))
                    return true;
            }
        }
        // 페인트 버킷 활성화 상태에서 빈 공간 클릭 시 페인트 통 작동
        if (g_paintMode) {
            auto worldPos = this->m_editorLayer->m_objectLayer->convertToNodeSpace(touch->getLocation());
            this->updateNearbyObjects(worldPos, 1500.0f);
            if (isWallAt(worldPos)) return true;
            this->Fill(worldPos);
            return true;
        }
        return EditorUI::ccTouchBegan(touch, event);
    }

    // 클릭한 점부터 위아래로 블록을 채워나가는 기능
    void Fill(CCPoint startPos) {
        auto layer = this->m_editorLayer;
        int paintChannel = registerColorChannel(g_paintColor);
        std::vector<GameObject*> thisBatch;

        for (float vDir : {1.0f, -1.0f}) {
            float seedX = startPos.x;
            int lineCount = 0;

            for (float dy = 0; std::abs(dy) < 1000.0f; dy += vDir) {
                if (++lineCount > 500) break;
                float curY = startPos.y + dy;
                if (dy != 0 && isWallAt({seedX, curY})) break;

                float left  = findEdge({seedX, curY}, -1.0f, 1000.0f);
                float right = findEdge({seedX, curY},  1.0f, 1000.0f);
                float width = right - left;
                if (width < 0.5f || width > 1200.0f) break;

                seedX = (left + right) / 2.0f;

                // [빌드 오류 수정] 생성 좌표 데이터 타입을 명확하게 CCPoint로 명시
                auto obj = layer->createObject(211, CCPoint{seedX, curY}, true);
                if (!obj) continue;

                float scaleX = width / 30.0f;
                float scaleY = 1.1f / 30.0f;

                obj->m_startScaleX = obj->m_customScaleX = scaleX;
                obj->m_startScaleY = obj->m_customScaleY = scaleY;
                obj->updateCustomScaleX(scaleX);
                obj->updateCustomScaleY(scaleY);

                // [빌드 오류 수정] Cocos 좌표 연산 시 대입 연산자(=) 모호성 에러 해결
                obj->setPosition(CCPoint{seedX, curY});
                obj->m_startPosition = CCPoint{seedX, curY};
                obj->m_editorLayer = layer->m_currentLayer;

                applyColorToObject(obj, paintChannel);
                layer->addToSection(obj);
                obj->retain();
                thisBatch.push_back(obj);
            }
        }

        if (!thisBatch.empty()) {
            m_fields->undoStack.push_back(thisBatch);
            m_fields->redoStack.clear();
        }
        layer->updateOptions();
    }
};
