#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <vector>
#include <cmath>

using namespace geode::prelude;

bool g_paintMode = false;
cocos2d::ccColor3B g_paintColor = {255, 255, 255};

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    void onStopPlaytest() {
        LevelEditorLayer::onStopPlaytest();
    }
};

class $modify(MyEditorUI, EditorUI) {
    struct Fields {
        std::vector<GameObject*> m_nearbyObjects;
        CCNode* m_paintTabContent = nullptr;
        bool m_paintTabOpen = false;
        CCTextInputNode* m_colorInput = nullptr;
        std::vector<std::vector<GameObject*>> m_undoStack;
        std::vector<std::vector<GameObject*>> m_redoStack;
    };

    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;

        if (auto tabMenu = this->getChildByID("build-tabs-menu")) {
            auto tabSprite = CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png");
            if (!tabSprite) tabSprite = CCSprite::create();
            tabSprite->setScale(0.8f);

            auto tabBtn = CCMenuItemSpriteExtra::create(
                tabSprite, this, menu_selector(MyEditorUI::onPaintTabClicked)
            );
            tabBtn->setID("paint-tab-button"_spr);
            tabMenu->addChild(tabBtn);
            tabMenu->updateLayout();
        }

        this->createPaintTabContent();
        return true;
    }

    void onPause(CCObject* sender) {
        g_paintMode = false;
        EditorUI::onPause(sender);
    }

    void createPaintTabContent() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto panel = CCNode::create();
        panel->setID("paint-tab-content"_spr);
        panel->setVisible(false);

        CCPoint menuPos = {winSize.width / 2, 570};
        if (auto tabMenu = this->getChildByID("build-tabs-menu")) {
            menuPos = tabMenu->getPosition();
            menuPos.y += 100;
        }

        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({500, 120});
        bg->setPosition(menuPos);
        bg->setOpacity(220);
        panel->addChild(bg);

        auto label = CCLabelBMFont::create("Color:", "bigFont.fnt");
        label->setScale(0.45f);
        label->setPosition({menuPos.x - 180, menuPos.y + 25});
        panel->addChild(label);

        auto inputBg = CCScale9Sprite::create("square02_001.png");
        inputBg->setContentSize({220, 36});
        inputBg->setPosition({menuPos.x + 30, menuPos.y + 25});
        inputBg->setOpacity(100);
        panel->addChild(inputBg);

        auto input = CCTextInputNode::create(210, 30, " ", "bigFont.fnt");
        input->setString("#FFFFFF");
        input->setMaxLabelScale(0.5f);
        input->setLabelPlaceholderScale(0.0f);
        input->setAllowedChars("#0123456789ABCDEFabcdef");
        input->setPosition({menuPos.x + 30, menuPos.y + 25});
        input->setID("color-input"_spr);
        panel->addChild(input);
        m_fields->m_colorInput = input;

        auto menu = CCMenu::create();
        menu->setPosition(menuPos);
        menu->setTouchPriority(-200);

        auto applyLabel = CCLabelBMFont::create("Apply", "bigFont.fnt");
        applyLabel->setScale(0.45f);
        auto applyBtn = CCMenuItemSpriteExtra::create(
            applyLabel, this, menu_selector(MyEditorUI::onApplyColor)
        );
        applyBtn->setPosition({-150, -25});

        auto paintSprite = CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png");
        if (!paintSprite) paintSprite = CCSprite::create();
        paintSprite->setScale(0.8f);
        auto paintBtn = CCMenuItemSpriteExtra::create(
            paintSprite, this, menu_selector(MyEditorUI::onPaintBucketClicked)
        );
        paintBtn->setPosition({-50, -25});

        auto undoLabel = CCLabelBMFont::create("Undo", "bigFont.fnt");
        undoLabel->setScale(0.45f);
        auto undoBtn = CCMenuItemSpriteExtra::create(
            undoLabel, this, menu_selector(MyEditorUI::onPaintUndo)
        );
        undoBtn->setPosition({50, -25});

        auto redoLabel = CCLabelBMFont::create("Redo", "bigFont.fnt");
        redoLabel->setScale(0.45f);
        auto redoBtn = CCMenuItemSpriteExtra::create(
            redoLabel, this, menu_selector(MyEditorUI::onPaintRedo)
        );
        redoBtn->setPosition({150, -25});

        menu->addChild(applyBtn);
        menu->addChild(paintBtn);
        menu->addChild(undoBtn);
        menu->addChild(redoBtn);
        panel->addChild(menu);

        this->addChild(panel, 100);
        m_fields->m_paintTabContent = panel;
    }

    bool parseHexColor(const std::string& hex, cocos2d::ccColor3B& out) {
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        if (h.size() != 6) return false;
        try {
            int r = std::stoi(h.substr(0, 2), nullptr, 16);
            int g = std::stoi(h.substr(2, 2), nullptr, 16);
            int b = std::stoi(h.substr(4, 2), nullptr, 16);
            out = {(GLubyte)r, (GLubyte)g, (GLubyte)b};
            return true;
        } catch (...) { return false; }
    }

    void onApplyColor(CCObject*) {
        if (!m_fields->m_colorInput) return;
        std::string hex = m_fields->m_colorInput->getString();
        cocos2d::ccColor3B color;
        if (parseHexColor(hex, color)) {
            g_paintColor = color;
            Notification::create("Color applied!", NotificationIcon::Success)->show();
        } else {
            Notification::create("Invalid! Use #RRGGBB", NotificationIcon::Error)->show();
        }
    }

    void onPaintTabClicked(CCObject*) {
        m_fields->m_paintTabOpen = !m_fields->m_paintTabOpen;
        if (m_fields->m_paintTabContent)
            m_fields->m_paintTabContent->setVisible(m_fields->m_paintTabOpen);
    }

    void onPaintBucketClicked(CCObject*) {
        g_paintMode = !g_paintMode;
        Notification::create(
            std::string("Paint Bucket ") + (g_paintMode ? "ON" : "OFF"),
            NotificationIcon::Info
        )->show();
    }

    void onPaintUndo(CCObject*) {
        if (m_fields->m_undoStack.empty()) {
            Notification::create("Nothing to undo!", NotificationIcon::Warning)->show();
            return;
        }

        auto layer = this->m_editorLayer;
        auto lastBatch = m_fields->m_undoStack.back();
        m_fields->m_undoStack.pop_back();

        for (auto* obj : lastBatch) {
            if (obj) {
                obj->setVisible(false);
                obj->m_isDisabled = true;
                layer->m_objects->removeObject(obj);
            }
        }

        m_fields->m_redoStack.push_back(lastBatch);
        layer->updateOptions();
        Notification::create("Undo!", NotificationIcon::Info)->show();
    }

    void onPaintRedo(CCObject*) {
        if (m_fields->m_redoStack.empty()) {
            Notification::create("Nothing to redo!", NotificationIcon::Warning)->show();
            return;
        }

        auto layer = this->m_editorLayer;
        auto lastBatch = m_fields->m_redoStack.back();
        m_fields->m_redoStack.pop_back();

        for (auto* obj : lastBatch) {
            if (obj) {
                obj->setVisible(true);
                obj->m_isDisabled = false;
                layer->m_objects->addObject(obj);
            }
        }

        m_fields->m_undoStack.push_back(lastBatch);
        layer->updateOptions();
        Notification::create("Redo!", NotificationIcon::Info)->show();
    }

    void updateNearbyObjects(CCPoint center, float range) {
        m_fields->m_nearbyObjects.clear();
        auto objects = this->m_editorLayer->m_objects;
        if (!objects) return;
        CCRect searchArea = { center.x - range, center.y - range, range * 2, range * 2 };
        for (auto* obj : CCArrayExt<GameObject*>(objects)) {
            if (searchArea.intersectsRect(obj->boundingBox()))
                m_fields->m_nearbyObjects.push_back(obj);
        }
    }

    bool isPointTrulyInside(GameObject* obj, CCPoint pos) {
        if (!obj->boundingBox().containsPoint(pos)) return false;
        float angle = obj->getRotation();
        CCPoint center = obj->getPosition();
        CCPoint localPos = pos - center;
        float rad = CC_DEGREES_TO_RADIANS(angle);
        float cosA = std::cos(rad), sinA = std::sin(rad);
        float lx = localPos.x * cosA - localPos.y * sinA;
        float ly = localPos.x * sinA + localPos.y * cosA;
        float hw = obj->getContentSize().width  * std::abs(obj->getScaleX()) / 2.0f;
        float hh = obj->getContentSize().height * std::abs(obj->getScaleY()) / 2.0f;
        return std::abs(lx) <= hw && std::abs(ly) <= hh;
    }

    bool isWallAt(CCPoint pos) {
        for (auto* obj : m_fields->m_nearbyObjects)
            if (isPointTrulyInside(obj, pos)) return true;
        return false;
    }

    float findTrueEdge(CCPoint start, float dir, float maxRange) {
        float x = start.x;
        const float precision = 0.5f;
        float dist = 0.0f;
        while (dist < maxRange) {
            if (isWallAt({x + dir * precision, start.y})) break;
            x += dir * precision;
            dist += precision;
        }
        return x;
    }

    int registerColorChannel(cocos2d::ccColor3B color) {
        auto levelSettings = this->m_editorLayer->m_levelSettings;
        if (!levelSettings) return -1;
        auto em = levelSettings->m_effectManager;
        if (!em) return -1;

        int channelID = levelSettings->m_nextFreeID;
        if (channelID <= 0) channelID = 1;
        levelSettings->m_nextFreeID = channelID + 1;

        auto ca = ColorAction::create(color, color, 0.0f, 0.0, false, 0, 1.0f, 1.0f);
        if (!ca) return -1;

        ca->m_color          = color;
        ca->m_fromColor      = color;
        ca->m_toColor        = color;
        ca->m_currentOpacity = 1.0f;
        ca->m_fromOpacity    = 1.0f;
        ca->m_toOpacity      = 1.0f;
        ca->m_stepFinished   = true;
        ca->m_playerColor    = 0;
        ca->m_blending       = false;

        em->setColorAction(ca, channelID);
        em->calculateBaseActiveColors();
        em->processColors();

        return channelID;
    }

    void applyColorToObject(GameObject* obj, int channelID) {
        if (channelID < 0) return;

        if (obj->m_baseColor) {
            obj->m_baseColor->m_colorID = channelID;
            obj->m_baseColor->m_usesHSV = false;
            obj->m_baseColor->m_opacity = 1.0f;
        } else {
            obj->setColor(g_paintColor);
        }

        if (obj->m_detailColor) {
            obj->m_detailColor->m_colorID = channelID;
            obj->m_detailColor->m_usesHSV = false;
            obj->m_detailColor->m_opacity = 1.0f;
        }

        obj->updateCustomColorType(1);
        obj->updateCustomColorType(2);

        auto em = this->m_editorLayer->m_levelSettings->m_effectManager;
        if (em) {
            auto ca = em->getColorAction(channelID);
            if (ca) em->colorActionChanged(ca);
        }
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (m_fields->m_paintTabOpen && m_fields->m_paintTabContent) {
            auto touchPos = touch->getLocation();
            auto children = m_fields->m_paintTabContent->getChildren();
            for (int i = 0; i < (int)children->count(); i++) {
                auto child = dynamic_cast<CCNode*>(children->objectAtIndex(i));
                if (child && child->boundingBox().containsPoint(
                        m_fields->m_paintTabContent->convertToNodeSpace(touchPos)))
                    return true;
            }
        }
        if (g_paintMode) {
            auto worldPos = this->m_editorLayer->m_objectLayer->convertToNodeSpace(touch->getLocation());
            this->updateNearbyObjects(worldPos, 1500.0f);
            if (isWallAt(worldPos)) return true;
            this->executeSmartFill(worldPos);
            return true;
        }
        return EditorUI::ccTouchBegan(touch, event);
    }

    void executeSmartFill(CCPoint startPos) {
        const float Y_STEP   = 1.0f;
        const float MAX_SCAN = 1000.0f;
        const float GD_UNIT  = 30.0f;
        auto layer = this->m_editorLayer;

        int paintChannel = registerColorChannel(g_paintColor);
        std::vector<GameObject*> thisBatch;

        for (float vDir : {1.0f, -1.0f}) {
            float seedX = startPos.x;
            int lineCount = 0;

            for (float dy = 0; std::abs(dy) < MAX_SCAN; dy += vDir * Y_STEP) {
                if (++lineCount > 500) break;
                float curY = startPos.y + dy;
                if (dy != 0 && isWallAt({seedX, curY})) break;

                float left  = findTrueEdge({seedX, curY}, -1.0f, MAX_SCAN);
                float right = findTrueEdge({seedX, curY},  1.0f, MAX_SCAN);
                float width = right - left;
                if (width < 0.5f || width > 1200.0f) break;

                float midX = (left + right) / 2.0f;
                seedX = midX;

                auto obj = layer->createObject(211, {midX, curY}, true);
                if (!obj) continue;

                float scaleX = width / GD_UNIT;
                float scaleY = (Y_STEP + 0.1f) / GD_UNIT;

                obj->m_startScaleX  = scaleX;
                obj->m_startScaleY  = scaleY;
                obj->m_customScaleX = scaleX;
                obj->m_customScaleY = scaleY;

                obj->updateCustomScaleX(scaleX);
                obj->updateCustomScaleY(scaleY);

                obj->setPosition(cocos2d::CCPoint(midX, curY));
                obj->m_startPosition = cocos2d::CCPoint(midX, curY);
                obj->m_editorLayer = layer->m_currentLayer;

                applyColorToObject(obj, paintChannel);
                layer->addToSection(obj);

                obj->retain();
                thisBatch.push_back(obj);
            }
        }

        if (!thisBatch.empty()) {
            m_fields->m_undoStack.push_back(thisBatch);
            m_fields->m_redoStack.clear();
        }

        auto em = layer->m_levelSettings->m_effectManager;
        if (em) {
            em->calculateBaseActiveColors();
            em->processColors();
        }

        layer->updateOptions();
    }
};