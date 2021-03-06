#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QShortcut>
#include <QVBoxLayout>

#include "Cache.h"
#include "ChatPage.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "Utils.h"
#include "dialogs/UserProfile.h"
#include "ui/Avatar.h"
#include "ui/FlatButton.h"

using namespace dialogs;

Q_DECLARE_METATYPE(std::vector<DeviceInfo>)

constexpr int BUTTON_SIZE       = 36;
constexpr int BUTTON_RADIUS     = BUTTON_SIZE / 2;
constexpr int WIDGET_MARGIN     = 20;
constexpr int TOP_WIDGET_MARGIN = 2 * WIDGET_MARGIN;
constexpr int WIDGET_SPACING    = 15;
constexpr int TEXT_SPACING      = 4;
constexpr int DEVICE_SPACING    = 5;

DeviceItem::DeviceItem(DeviceInfo device, QWidget *parent)
  : QWidget(parent)
  , info_(std::move(device))
{
        QFont font;
        font.setBold(true);

        auto deviceIdLabel = new QLabel(info_.device_id, this);
        deviceIdLabel->setFont(font);

        auto layout = new QVBoxLayout{this};
        layout->addWidget(deviceIdLabel);

        if (!info_.display_name.isEmpty())
                layout->addWidget(new QLabel(info_.display_name, this));

        layout->setMargin(0);
        layout->setSpacing(4);
}

UserProfile::UserProfile(QWidget *parent)
  : QWidget(parent)
{
        setAutoFillBackground(true);
        setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose, true);

        QIcon banIcon, kickIcon, ignoreIcon, startChatIcon;

        banIcon.addFile(":/icons/icons/ui/do-not-disturb-rounded-sign.png");
        banBtn_ = new FlatButton(this);
        banBtn_->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        banBtn_->setCornerRadius(BUTTON_RADIUS);
        banBtn_->setIcon(banIcon);
        banBtn_->setIconSize(QSize(BUTTON_RADIUS, BUTTON_RADIUS));
        banBtn_->setToolTip(tr("Ban the user from the room"));

        ignoreIcon.addFile(":/icons/icons/ui/volume-off-indicator.png");
        ignoreBtn_ = new FlatButton(this);
        ignoreBtn_->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        ignoreBtn_->setCornerRadius(BUTTON_RADIUS);
        ignoreBtn_->setIcon(ignoreIcon);
        ignoreBtn_->setIconSize(QSize(BUTTON_RADIUS, BUTTON_RADIUS));
        ignoreBtn_->setToolTip(tr("Ignore messages from this user"));
        ignoreBtn_->setDisabled(true); // Not used yet.

        kickIcon.addFile(":/icons/icons/ui/round-remove-button.png");
        kickBtn_ = new FlatButton(this);
        kickBtn_->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        kickBtn_->setCornerRadius(BUTTON_RADIUS);
        kickBtn_->setIcon(kickIcon);
        kickBtn_->setIconSize(QSize(BUTTON_RADIUS, BUTTON_RADIUS));
        kickBtn_->setToolTip(tr("Kick the user from the room"));

        startChatIcon.addFile(":/icons/icons/ui/black-bubble-speech.png");
        startChat_ = new FlatButton(this);
        startChat_->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        startChat_->setCornerRadius(BUTTON_RADIUS);
        startChat_->setIcon(startChatIcon);
        startChat_->setIconSize(QSize(BUTTON_RADIUS, BUTTON_RADIUS));
        startChat_->setToolTip(tr("Start a conversation"));

        connect(startChat_, &QPushButton::clicked, this, [this]() {
                auto user_id = userIdLabel_->text();

                mtx::requests::CreateRoom req;
                req.preset     = mtx::requests::Preset::PrivateChat;
                req.visibility = mtx::requests::Visibility::Private;

                if (utils::localUser() != user_id)
                        req.invite = {user_id.toStdString()};

                if (QMessageBox::question(
                      this,
                      tr("Confirm DM"),
                      tr("Do you really want to invite %1 (%2) to a direct chat?")
                        .arg(cache::displayName(roomId_, user_id))
                        .arg(user_id)) != QMessageBox::Yes)
                        return;

                emit ChatPage::instance()->createRoom(req);
        });

        connect(banBtn_, &QPushButton::clicked, this, [this] {
                ChatPage::instance()->banUser(userIdLabel_->text(), "");
        });
        connect(kickBtn_, &QPushButton::clicked, this, [this] {
                ChatPage::instance()->kickUser(userIdLabel_->text(), "");
        });

        // Button line
        auto btnLayout = new QHBoxLayout;
        btnLayout->addStretch(1);
        btnLayout->addWidget(startChat_);
        btnLayout->addWidget(ignoreBtn_);

        btnLayout->addWidget(kickBtn_);
        btnLayout->addWidget(banBtn_);
        btnLayout->addStretch(1);
        btnLayout->setSpacing(8);
        btnLayout->setMargin(0);

        avatar_ = new Avatar(this, 128);
        avatar_->setLetter("X");

        QFont font;
        font.setPointSizeF(font.pointSizeF() * 2);

        userIdLabel_      = new QLabel(this);
        displayNameLabel_ = new QLabel(this);
        displayNameLabel_->setFont(font);

        auto textLayout = new QVBoxLayout;
        textLayout->addWidget(displayNameLabel_);
        textLayout->addWidget(userIdLabel_);
        textLayout->setAlignment(displayNameLabel_, Qt::AlignCenter | Qt::AlignTop);
        textLayout->setAlignment(userIdLabel_, Qt::AlignCenter | Qt::AlignTop);
        textLayout->setSpacing(TEXT_SPACING);
        textLayout->setMargin(0);

        devices_ = new QListWidget{this};
        devices_->setFrameStyle(QFrame::NoFrame);
        devices_->setSelectionMode(QAbstractItemView::NoSelection);
        devices_->setAttribute(Qt::WA_MacShowFocusRect, 0);
        devices_->setSpacing(DEVICE_SPACING);

        QFont descriptionLabelFont;
        descriptionLabelFont.setWeight(65);

        devicesLabel_ = new QLabel(tr("Devices").toUpper(), this);
        devicesLabel_->setFont(descriptionLabelFont);
        devicesLabel_->hide();
        devicesLabel_->setFixedSize(devicesLabel_->sizeHint());

        auto okBtn = new QPushButton("OK", this);

        auto closeLayout = new QHBoxLayout();
        closeLayout->setSpacing(15);
        closeLayout->addStretch(1);
        closeLayout->addWidget(okBtn);

        auto vlayout = new QVBoxLayout{this};
        vlayout->addWidget(avatar_, 0, Qt::AlignCenter | Qt::AlignTop);
        vlayout->addLayout(textLayout);
        vlayout->addLayout(btnLayout);
        vlayout->addWidget(devicesLabel_, 0, Qt::AlignLeft);
        vlayout->addWidget(devices_, 1);
        vlayout->addLayout(closeLayout);

        QFont largeFont;
        largeFont.setPointSizeF(largeFont.pointSizeF() * 1.5);

        setMinimumWidth(
          std::max(devices_->sizeHint().width() + 4 * WIDGET_MARGIN, conf::window::minModalWidth));
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

        vlayout->setSpacing(WIDGET_SPACING);
        vlayout->setContentsMargins(WIDGET_MARGIN, TOP_WIDGET_MARGIN, WIDGET_MARGIN, WIDGET_MARGIN);

        qRegisterMetaType<std::vector<DeviceInfo>>();

        auto closeShortcut = new QShortcut(QKeySequence(QKeySequence::Cancel), this);
        connect(closeShortcut, &QShortcut::activated, this, &UserProfile::close);
        connect(okBtn, &QPushButton::clicked, this, &UserProfile::close);
}

void
UserProfile::resetToDefaults()
{
        avatar_->setLetter("X");
        devices_->clear();

        ignoreBtn_->show();
        devices_->hide();
        devicesLabel_->hide();
}

void
UserProfile::init(const QString &userId, const QString &roomId)
{
        resetToDefaults();

        this->roomId_ = roomId;

        auto displayName = cache::displayName(roomId, userId);

        userIdLabel_->setText(userId);
        displayNameLabel_->setText(displayName);
        avatar_->setLetter(utils::firstChar(displayName));

        avatar_->setImage(roomId, userId);

        auto localUser = utils::localUser();

        try {
                bool hasMemberRights =
                  cache::hasEnoughPowerLevel({mtx::events::EventType::RoomMember},
                                             roomId.toStdString(),
                                             localUser.toStdString());
                if (!hasMemberRights) {
                        kickBtn_->hide();
                        banBtn_->hide();
                } else {
                        kickBtn_->show();
                        banBtn_->show();
                }
        } catch (const lmdb::error &e) {
                nhlog::db()->warn("lmdb error: {}", e.what());
        }

        if (localUser == userId) {
                // TODO: click on display name & avatar to change.
                kickBtn_->hide();
                banBtn_->hide();
                ignoreBtn_->hide();
        }

        mtx::requests::QueryKeys req;
        req.device_keys[userId.toStdString()] = {};

        // A proxy object is used to emit the signal instead of the original object
        // which might be destroyed by the time the http call finishes.
        auto proxy = std::make_shared<Proxy>();
        QObject::connect(proxy.get(), &Proxy::done, this, &UserProfile::updateDeviceList);

        http::client()->query_keys(
          req,
          [user_id = userId.toStdString(), proxy = std::move(proxy)](
            const mtx::responses::QueryKeys &res, mtx::http::RequestErr err) {
                  if (err) {
                          nhlog::net()->warn("failed to query device keys: {} {}",
                                             err->matrix_error.error,
                                             static_cast<int>(err->status_code));
                          // TODO: Notify the UI.
                          return;
                  }

                  if (res.device_keys.empty() ||
                      (res.device_keys.find(user_id) == res.device_keys.end())) {
                          nhlog::net()->warn("no devices retrieved {}", user_id);
                          return;
                  }

                  auto devices = res.device_keys.at(user_id);

                  std::vector<DeviceInfo> deviceInfo;
                  for (const auto &d : devices) {
                          auto device = d.second;

                          // TODO: Verify signatures and ignore those that don't pass.
                          deviceInfo.emplace_back(DeviceInfo{
                            QString::fromStdString(d.first),
                            QString::fromStdString(device.unsigned_info.device_display_name)});
                  }

                  std::sort(deviceInfo.begin(),
                            deviceInfo.end(),
                            [](const DeviceInfo &a, const DeviceInfo &b) {
                                    return a.device_id > b.device_id;
                            });

                  if (!deviceInfo.empty())
                          emit proxy->done(QString::fromStdString(user_id), deviceInfo);
          });
}

void
UserProfile::updateDeviceList(const QString &user_id, const std::vector<DeviceInfo> &devices)
{
        if (user_id != userIdLabel_->text())
                return;

        for (const auto &dev : devices) {
                auto deviceItem = new DeviceItem(dev, this);
                auto item       = new QListWidgetItem;

                item->setSizeHint(deviceItem->minimumSizeHint());
                item->setFlags(Qt::NoItemFlags);
                item->setTextAlignment(Qt::AlignCenter);

                devices_->insertItem(devices_->count() - 1, item);
                devices_->setItemWidget(item, deviceItem);
        }

        devicesLabel_->show();
        devices_->show();
        adjustSize();
}
