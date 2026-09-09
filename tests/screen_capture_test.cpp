#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/ScreenObservationCoordinator.h"
#include "infrastructure/ScreenCapture.h"
#include "infrastructure/ScreenFingerprint.h"

namespace zhu_screen_pet {

class ScreenCaptureTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<CapturedImage>("CapturedImage");
    }

    void manualCapturePersistsCompressedImage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScreenCapture capture;
        ImageCompressionOptions options;
        options.maxWidth = 640;
        options.quality = 60;
        capture.configure(false, 30000, directory.path(), options);
        QSignalSpy capturedSpy(&capture, &ScreenCapture::captured);
        QString error;
        if (!capture.captureNow(&error)) {
            QSKIP(qPrintable(QStringLiteral("screen capture unavailable in test environment: %1")
                                 .arg(error)));
        }
        QCOMPARE(capturedSpy.count(), 1);
        const CapturedImage image = qvariant_cast<CapturedImage>(capturedSpy.at(0).at(0));
        QVERIFY(!image.data.isEmpty());
        QVERIFY(!image.filePath.isEmpty());
        QVERIFY(QFileInfo::exists(image.filePath));
        QVERIFY(image.size.width() <= 640);
        QVERIFY(capture.clearCaptures(&error));
        QVERIFY(!QFileInfo::exists(image.filePath));
    }

    void automaticUploadIsOffByDefaultAndRemainsMemoryOnly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScreenObservationCoordinator coordinator;
        coordinator.setCaptureDirectory(directory.path());
        coordinator.setObservationReady(true);
        UiConfig config;
        config.screenCaptureEnabled = true;
        QVERIFY(!config.automaticScreenAnalysisEnabled);
        coordinator.applyConfiguration(config);
        QSignalSpy readySpy(&coordinator,
                            &ScreenObservationCoordinator::scheduledImageReady);
        CapturedImage image;
        image.data = QByteArrayLiteral("image");
        image.trigger = CaptureTrigger::Scheduled;
        emit coordinator.screenCapture()->captured(image);
        QCOMPARE(readySpy.count(), 0);
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 0);
    }

    void scheduledCaptureStaysInMemory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScreenCapture capture;
        ImageCompressionOptions options;
        capture.configure(true, 30000, directory.path(), options);
        CapturedImage image;
        QString error;
        if (!capture.captureImage(&image, &error, CaptureTrigger::Scheduled)) {
            QSKIP(qPrintable(QStringLiteral("screen capture unavailable in test environment: %1")
                                 .arg(error)));
        }
        QVERIFY(!image.data.isEmpty());
        QVERIFY(image.filePath.isEmpty());
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 0);
    }

    void fingerprintResetsAcrossConversationAndAutomaticRestart()
    {
        ScreenObservationCoordinator coordinator;
        coordinator.setObservationReady(true);
        UiConfig config;
        config.screenCaptureEnabled = true;
        config.automaticScreenAnalysisEnabled = true;
        coordinator.applyConfiguration(config);
        coordinator.setObservationScope(QStringLiteral("conversation-a"));
        QSignalSpy readySpy(&coordinator,
                            &ScreenObservationCoordinator::scheduledImageReady);
        CapturedImage image;
        image.data = QByteArrayLiteral("image");
        image.fingerprint = QByteArray(
            (ScreenFingerprint::Width * ScreenFingerprint::Height + 7) / 8, '\0');
        image.trigger = CaptureTrigger::Scheduled;

        emit coordinator.screenCapture()->captured(image);
        QCOMPARE(readySpy.count(), 1);
        coordinator.finishScheduledRequest();
        emit coordinator.screenCapture()->captured(image);
        QCOMPARE(readySpy.count(), 1);

        coordinator.setObservationScope(QStringLiteral("conversation-b"));
        emit coordinator.screenCapture()->captured(image);
        QCOMPARE(readySpy.count(), 2);
        coordinator.finishScheduledRequest();

        config.automaticScreenAnalysisEnabled = false;
        coordinator.applyConfiguration(config);
        config.automaticScreenAnalysisEnabled = true;
        coordinator.applyConfiguration(config);
        emit coordinator.screenCapture()->captured(image);
        QCOMPARE(readySpy.count(), 3);
    }

    void fingerprintUsesThreePercentChangeThreshold()
    {
        constexpr int bitCount = ScreenFingerprint::Width * ScreenFingerprint::Height;
        const int byteCount = (bitCount + 7) / 8;
        const QByteArray baseline(byteCount, '\0');
        const auto changedBits = [byteCount](int count) {
            QByteArray fingerprint(byteCount, '\0');
            for (int bit = 0; bit < count; ++bit) {
                fingerprint[bit / 8] = static_cast<char>(
                    static_cast<unsigned char>(fingerprint.at(bit / 8))
                    | static_cast<unsigned char>(1U << (bit % 8)));
            }
            return fingerprint;
        };
        QVERIFY(!ScreenFingerprint::hasSignificantChange(baseline, changedBits(69)));
        QVERIFY(ScreenFingerprint::hasSignificantChange(baseline, changedBits(70)));

        QImage ascending(ScreenFingerprint::Width + 1, ScreenFingerprint::Height,
                         QImage::Format_Grayscale8);
        QImage descending = ascending;
        for (int y = 0; y < ScreenFingerprint::Height; ++y) {
            uchar* ascendingRow = ascending.scanLine(y);
            uchar* descendingRow = descending.scanLine(y);
            for (int x = 0; x <= ScreenFingerprint::Width; ++x) {
                ascendingRow[x] = static_cast<uchar>(x * 3);
                descendingRow[x] = static_cast<uchar>((ScreenFingerprint::Width - x) * 3);
            }
        }
        const QByteArray first = ScreenFingerprint::create(ascending);
        QCOMPARE(first.size(), byteCount);
        QCOMPARE(ScreenFingerprint::differenceRatio(first, first), 0.0);
        QCOMPARE(ScreenFingerprint::differenceRatio(
                     first, ScreenFingerprint::create(descending)), 1.0);
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::ScreenCaptureTest)
#include "screen_capture_test.moc"
