#include "rsdevice.h"

#include <QDebug>
#include <QLoggingCategory>
#include <limits>

#include "rsmanager.h"

//#include "camera/pointcloud.h"
//#include "settings.h"

Q_LOGGING_CATEGORY(rsdevice, "RealSensePlugin::RSDevice")

const unsigned int FRAME_QUEUE_SIZE = std::numeric_limits<unsigned int>::max();

RSDevice::RSDevice(RSManager *rsmanager, const QString serial_number)
    : QObject()
    , m_rsmanager(rsmanager)
    , m_serial_number(serial_number)
    , m_pipe(nullptr)
    , m_queue(FRAME_QUEUE_SIZE)
    , m_generator(m_pipe, &m_queue, this)
{
    qCDebug(rsdevice) << "Create new" << m_serial_number;
    init();

    m_generator.moveToThread(&m_generator_thread);

    connect(this, &RSDevice::started, &m_generator, &RSDeviceWorker::doWork);

    connect(&m_generator, &RSDeviceWorker::newDepthImage,
            this, &RSDevice::onNewDepthImage,
            Qt::ConnectionType::QueuedConnection);

    /*connect(&m_generator, &RSDeviceWorker::newPointCloud,
            this, &RSDevice::onNewPointCloud,
            Qt::ConnectionType::QueuedConnection);*/

    connect(&m_generator, &RSDeviceWorker::stopped, this, &RSDevice::_stop);

    //connect(&m_generator, &RSDeviceWorker::errorOccurred, this, &RSDevice::onGeneratorErrorOccurred);

    /*connect(&m_generator, &RSDeviceWorker::streamFPS, this, &RSDevice::setStreamFPS);
    connect(&m_generator, &RSDeviceWorker::streamFWT, this, &RSDevice::setStreamFWT);
    connect(&m_generator, &RSDeviceWorker::streamFPT, this, &RSDevice::setStreamFPT);*/

    m_generator_thread.start();
    qCDebug(rsdevice) << "Created" << m_serial_number;
}

RSDevice::~RSDevice()
{
    stop();
    m_generator_thread.quit();
    m_generator_thread.wait();
    deinit();
}

VideoSourceStreamObject* RSDevice::connectStream(const QStringList path)
{
    qCDebug(rsdevice) << __func__ << "Setting stream parameters";

    for( VideoSourceStreamObject* stream : m_video_streams ) {
        if( stream->path() == path )
            return stream;
    }

    rs2::stream_profile sp = m_rsmanager->getStreamProfile(path);
    auto vp = sp.as<rs2::video_stream_profile>();
    m_config.enable_stream(sp.stream_type(), vp.width(), vp.height(), sp.format(), sp.fps());
    QStringList description;
    description << QString("%1 (USB:%2 FW:%3)")
            .arg(m_rsmanager->getDeviceInfo(m_serial_number, RS2_CAMERA_INFO_NAME))
            .arg(m_rsmanager->getDeviceInfo(m_serial_number, RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
            .arg(m_rsmanager->getDeviceInfo(m_serial_number, RS2_CAMERA_INFO_FIRMWARE_VERSION));
    VideoSourceStreamObject *stream = new VideoSourceStreamObject(path, description, sp.stream_type());
    m_video_streams.append(stream);

    start();

    return stream;
}

void RSDevice::start()
{
    if( getIsStreaming() )
        return;

    qCDebug(rsdevice) << "start streaming" << m_serial_number;

    if( m_pipe != nullptr ) {
        qCDebug(rsdevice) << "start pipe";
        try {
            m_profile = m_pipe->start(m_config);
            setIsStreaming(true);
            emit started();
        } catch( rs2::error e ) {
            qCWarning(rsdevice) << "Unable to start pipe with the current configuration:" << e.what();
            qCWarning(rsdevice) << "Disabling the color stream and retry";
            //m_config.disable_stream(RS2_STREAM_COLOR);
            //m_profile = m_pipe->start(m_config);
        }
    }
}

void RSDevice::stop()
{
    if( !getIsStreaming() )
        return;

    qCDebug(rsdevice) << "Stop generator for" << m_serial_number;
    m_generator.stop();
}

void RSDevice::_stop()
{
    if( m_pipe != nullptr ) {
        qCDebug(rsdevice) << "stop pipe";
        m_pipe->stop();
        qCDebug(rsdevice) << "after pipe stop";
        setIsStreaming(false);
    }
}

void RSDevice::makeShot()
{
    // TODO: move to settings
    /*try {
        qCDebug(rsdevice) << "set laser max";
        auto depth_sensor = m_profile.get_device().first<rs2::depth_sensor>();
        if( depth_sensor.supports(RS2_OPTION_LASER_POWER) )
        {
            // Query min and max values:
            auto range = depth_sensor.get_option_range(RS2_OPTION_LASER_POWER);
            qCDebug(rsdevice) << "Current laser power: " << depth_sensor.get_option(RS2_OPTION_LASER_POWER);
            qCDebug(rsdevice) << "range.step is" << range.step;
            if( depth_sensor.get_option(RS2_OPTION_LASER_POWER) < 299.0f ) {
                qCDebug(rsdevice) << "Set laser to" << range.max -range.step;
                depth_sensor.set_option(RS2_OPTION_LASER_POWER, 300.0f); // Set max power
            }
        }
    } catch( rs2::invalid_value_error e ) {
        qCWarning(rsdevice) << "Found invalid value during set max power" << e.get_failed_function().c_str() << e.what();
    }*/

    if( getIsStreaming() )
        m_generator.makeShot();
}

void RSDevice::init()
{
    qCDebug(rsdevice) << "Init device" << m_serial_number;

    if( m_pipe == nullptr )
        m_pipe = new rs2::pipeline();
    m_generator.setPipeline(m_pipe);

    m_config.enable_device(m_serial_number.toStdString());
    m_config.disable_all_streams();
}

void RSDevice::deinit()
{
    qCDebug(rsdevice) << "Deinitializing:" << m_serial_number;

    stop();
    setIsConnected(false);

    if( m_pipe != nullptr )
        delete m_pipe;
}

void RSDevice::onNewDepthImage(QImage image)
{
    //emit newDepthImage(image);
}

/*void RSDevice::onNewPointCloud(PointCloud *pc)
{
    qCDebug(rsdevice) << "Adding new pointcloud to the list";
    addPointCloud(pc);
}*/

/*void RSDevice::onGeneratorErrorOccurred(const QString &error)
{
    qCDebug(rsdevice) << error;
    stop();
    setIsStreaming(false);
    setIsScanning(false);
    emit errorOccurred(error);
}*/