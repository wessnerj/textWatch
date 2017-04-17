#include <tizen.h>
#include <sensor.h>
#include <device/battery.h>
#include <dlog.h>

#include <string.h>

#include "textwatch.h"

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;

	Evas_Object *Text_Intro;
	Evas_Object *Text_Prefix;
	Evas_Object *Text_Hour;

	Evas_Object *Text_Clock;

	Evas_Object *Text_Steps;

	Evas_Object *Text_Pulse;

	Evas_Object *Text_Battery;

	int currentBackground;

	/// Number is walked steps
	int currentStepCount;
	/// Walked distance [m]
	float walkedDistance;
	/// Current pulse [rpm]
	int currentPulse;

	sensor_listener_h pedometerListener;
	sensor_listener_h heartrateListener;

	int sumHrmMeasurements;
	int countHrmMeasurements;

	Ecore_Timer*	hrmTimer;
} appdata_s;

/// Maximum size of string buffers
const int TEXT_BUF_SIZE = 256;

/// Number of required heartrate measurements
const int REQ_HRM_MEASUREMENTS = 40;

/// Names of the different hours
const char* timeNames[12] = {
		"ZWÖLF",
		"EINS",
		"ZWEI",
		"DREI",
		"VIER",
		"FÜNF",
		"SECHS",
		"SIEBEN",
		"ACHT",
		"NEUN",
		"ZEHN",
		"ELF"
};

/// Short names of weekdays
const char* dayNames[7] = {
		"So",
		"Mo",
		"Di",
		"Mi",
		"Do",
		"Fr",
		"Sa"
};

/*
 * @brief Get path of resource.
 * @param[in] file_in File name
 * @param[out] file_path_out The point to which save full path of the resource
 * @param[in] file_path_max Size of file name include path
 */
static void data_get_resource_path(const char *file_in, char *file_path_out, int file_path_max)
{
	char *res_path = app_get_resource_path();
	if (res_path) {
		snprintf(file_path_out, file_path_max, "%s%s", res_path, file_in);
		free(res_path);
	}
}

static void update_background(appdata_s *ad, int hour)
{
	char* imageName;
	int newBackground;
	if (hour > 20 || hour < 3)
	{
		// Use night background
		imageName = "ch_bg_c_04.png";
		newBackground = 4;
	}
	else if (hour < 9)
	{
		// Use morning background
		imageName = "ch_bg_c_01.png";
		newBackground = 1;
	}
	else if (hour < 15)
	{
		// Use midday background
		imageName = "ch_bg_c_02.png";
		newBackground = 2;
	}
	else
	{
		// Use afternoon background
		imageName = "ch_bg_c_03.png";
		newBackground = 3;
	}

	if (ad->currentBackground == newBackground)
	{
		// nothing to do
		return;
	}

	// Get image path
	char bg_path[4096];
	data_get_resource_path(imageName, bg_path, sizeof(bg_path));

	// Set image
	elm_bg_file_set(ad->conform, bg_path, NULL);
	ad->currentBackground = newBackground;
}

static void update_watch(appdata_s *ad, watch_time_h watch_time, int ambient)
{
	// Return immediatly if no time is given
	if (watch_time == NULL)
		return;

	// Get buffer for strings to display
	char watch_text[TEXT_BUF_SIZE];

	// Get information about the current time
	int hour12, hour24, minute, second;
	watch_time_get_hour(watch_time, &hour12);
	watch_time_get_hour24(watch_time, &hour24);
	watch_time_get_minute(watch_time, &minute);
	watch_time_get_second(watch_time, &second);

	// Get information about the date
	int dayOfWeek, dayOfMonth, month, year;
	watch_time_get_day_of_week(watch_time, &dayOfWeek);
	// decrement by one (watch_time_get_day_of_week starts counting with 1 .. -.-)
	--dayOfWeek;
	watch_time_get_day(watch_time, &dayOfMonth);
	watch_time_get_month(watch_time, &month);
	watch_time_get_year(watch_time, &year);

	// Get the time prefix string
	char* prefixStr;
	if (minute < 3)
	{
		prefixStr = "genau";
	}
	else if (minute < 8)
	{
		prefixStr = "fünf nach";
	}
	else if (minute < 13)
	{
		prefixStr = "zehn nach";
	}
	else if (minute < 18)
	{
		prefixStr = "viertel nach";
	}
	else if (minute < 23)
	{
		prefixStr = "zwanzig nach";
	}
	else if (minute < 28)
	{
		prefixStr = "fünf vor halb";
		++hour12;
	}
	else if (minute < 33)
	{
		prefixStr = "halb";
		++hour12;
	}
	else if (minute < 38)
	{
		prefixStr = "fünf nach halb";
		++hour12;
	}
	else if (minute < 43)
	{
		prefixStr = "zwanzig vor";
		++hour12;
	}
	else if (minute < 48)
	{
		prefixStr = "dreiviertel";
		++hour12;
	}
	else if (minute < 53)
	{
		prefixStr = "zehn vor";
		++hour12;
	}
	else if (minute < 58)
	{
		prefixStr = "fünf vor";
		++hour12;
	}
	else
	{
		prefixStr = "genau";
		++hour12;
	}

	// Take care of the special case, where we added one to 12 o'clock ..
	if (hour12 >= 12)
		hour12 -= 12;

	// Set the time prefix text
	snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Bold font_size=36>%s</font></align>",
			prefixStr);
	elm_object_text_set(ad->Text_Prefix, watch_text);

	// Set the hour
	snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Bold font_size=42>%s</font></align>",
			timeNames[hour12]);
	elm_object_text_set(ad->Text_Hour, watch_text);

	// Set the digital time & date ..
	if (!ambient)
	{
		snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Regular font_size=32>%02d:%02d:%02d, %2s, %02d.%02d.%04d</font></align>",
			hour24, minute, second,
			dayNames[dayOfWeek], dayOfMonth, month, year);
	}
	else
	{
		// In ambient mode we skip the seconds ..
		snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Regular font_size=32>%02d:%02d, %2s, %02d.%02d.%04d</font></align>",
				hour24, minute,
				dayNames[dayOfWeek], dayOfMonth, month, year);
	}
	elm_object_text_set(ad->Text_Clock, watch_text);

	// Set the step count
	snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Regular font_size=28>Schritte: %d (%.1f km)</font></align>",
		ad->currentStepCount, (.001f * ad->walkedDistance));
	elm_object_text_set(ad->Text_Steps, watch_text);

	// Set the pulse data
	snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Regular font_size=28>Puls: %d/min</font></align>",
		ad->currentPulse);
	elm_object_text_set(ad->Text_Pulse, watch_text);

	// Set the battery data
	int bat;
	device_battery_get_percent(&bat);
	snprintf(watch_text, TEXT_BUF_SIZE, "<align=center><font=Tizen:style=Regular font_size=28>Batterie: %d%%</font></align>",
		bat);
	elm_object_text_set(ad->Text_Battery, watch_text);

	// Update background
	update_background(ad, hour24);
}

static void create_base_gui(appdata_s *ad, int width, int height)
{
	int ret;
	watch_time_h watch_time = NULL;

	// Get the main window
	ret = watch_app_get_elm_win(&ad->win);
	if (ret != APP_ERROR_NONE)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get window. Err = %d", ret);
		return;
	}
	evas_object_resize(ad->win, width, height);

	// Create conformant as background :)
	char bg_path[4096];
	data_get_resource_path("ch_bg_c_04.png", bg_path, sizeof(bg_path));

	ad->conform = elm_bg_add(ad->win);
	ret = elm_bg_file_set(ad->conform, bg_path, NULL);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to set background. err = %d", ret);
	}

	elm_bg_option_set(ad->conform, ELM_BG_OPTION_CENTER);

	evas_object_move(ad->conform, 0, 0);
	evas_object_resize(ad->conform, width, height);
	evas_object_show(ad->conform);

	// Keep track of the current y position
	int currY = 15;

	// Label for the introduction
	ad->Text_Intro = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Intro, 110, 40);
	evas_object_move(ad->Text_Intro, 125, currY);
	evas_object_show(ad->Text_Intro);
	elm_object_text_set(ad->Text_Intro, "<align=center><font=Tizen:style=Regular font_size=36>Es ist</font></align>");
	currY += 40;

	// Label for the time prefix
	ad->Text_Prefix = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Prefix, 240, 50);
	evas_object_move(ad->Text_Prefix, 60, currY);
	evas_object_show(ad->Text_Prefix);
	elm_object_text_set(ad->Text_Prefix, "<align=center><font=Tizen:style=Regular font_size=36>fünf vor halb</font></align>");
	currY += 50;

	// Label for the current hour
	ad->Text_Hour = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Hour, 182, 60);
	evas_object_move(ad->Text_Hour, 89, currY);
	evas_object_show(ad->Text_Hour);
	elm_object_text_set(ad->Text_Hour, "<align=center><font=Tizen:style=Regular font_size=42>ZWÖLF</font></align>");
	currY += 60;

	// Label for digital time & date
	ad->Text_Clock = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Clock, 360, 40);
	evas_object_move(ad->Text_Clock, 0, currY);
	evas_object_show(ad->Text_Clock);
	elm_object_text_set(ad->Text_Clock, "<align=center><font=Tizen:style=Regular font_size=32>11:25:30 Mo, 01.01.2001</font></align>");
	currY += 40;

	// Label for Step count
	ad->Text_Steps = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Steps, 280, 40);
	evas_object_move(ad->Text_Steps, 40, currY);
	evas_object_show(ad->Text_Steps);
	elm_object_text_set(ad->Text_Steps, "<align=center><font=Tizen:style=Regular font_size=28>Steps: 0 (0.0 km)</font></align>");
	currY += 40;

	// Label for pulse count
	ad->Text_Pulse = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Pulse, 200, 40);
	evas_object_move(ad->Text_Pulse, 80, currY);
	evas_object_show(ad->Text_Pulse);
	elm_object_text_set(ad->Text_Pulse, "<align=center><font=Tizen:style=Regular font_size=28>Pulse: 0</font></align>");
	currY += 40;

	// Label for battery status
	ad->Text_Battery = elm_label_add(ad->conform);
	evas_object_resize(ad->Text_Battery, 180, 40);
	evas_object_move(ad->Text_Battery, 90, currY);
	evas_object_show(ad->Text_Battery);
	elm_object_text_set(ad->Text_Battery, "<align=center><font=Tizen:style=Regular font_size=28>Batterie: 100%</font></align>");
	currY += 40;

	// Call update_watch to have the right time right at start-up:
	ret = watch_time_get_current_time(&watch_time);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d", ret);

	update_watch(ad, watch_time, 0);
	watch_time_delete(watch_time);

	// Show window after base gui is set up
	evas_object_show(ad->win);
}

/**
 * @brief Callback invoked by SENSOR_HUMAN_PEDOMETER listener.
 * @param sensor The sensor's handle.
 * @param event The event data.
 * @param data The user data.
 */
static void pedometer_cb(sensor_h sensor, sensor_event_s *event, void *data)
{
	appdata_s* ad = (appdata_s*) data;

	if (event->value_count < 4)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "Pedometer sensor is not delivering data!");
		return;
	}

	// Save current steps:
	// 0: Number of steps of current day
	// 3: Walked distance [m]
	ad->currentStepCount = (int) event->values[0];
	ad->walkedDistance = event->values[3];

//	for (int i = 0; i < event->value_count; ++i) {
//		dlog_print(DLOG_INFO, LOG_TAG, "pedometer_cb> %d -> %f", i, event->values[i]);
//	}
}

/**
 * @brief Callback invoked by SENSOR_HUMAN_PEDOMETER listener.
 * @param sensor The sensor's handle.
 * @param event The event data.
 * @param data The user data.
 */
static void heartrate_cb(sensor_h sensor, sensor_event_s *event, void *data)
{
	appdata_s* ad = (appdata_s*) data;

	if (event->value_count < 1)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "Pedometer sensor is not delivering data!");
		return;
	}

	// Save current steps
	if (event->values[0] > 10.f)
	{
		++ad->countHrmMeasurements;
		ad->sumHrmMeasurements += (int) event->values[0];
	}

	if (ad->countHrmMeasurements >= REQ_HRM_MEASUREMENTS)
	{
		// Set current pulse
		ad->currentPulse = ad->sumHrmMeasurements / ad->countHrmMeasurements;

		dlog_print(DLOG_INFO, LOG_TAG, "Save current pulse: %d", ad->currentPulse);

		// Unregister listener
		int ret = sensor_listener_stop(ad->heartrateListener);
		if (ret != SENSOR_ERROR_NONE) {
			dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_stop() error: %s", __FILE__, __LINE__, get_error_message(ret));
		}
	}
}

static void register_pedometer_listener(appdata_s *ad)
{
	sensor_h handle;
	int ret = sensor_get_default_sensor(SENSOR_HUMAN_PEDOMETER, &handle);
	if (ret != SENSOR_ERROR_NONE)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_get_default_sensor() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}

	ret = sensor_create_listener(handle, &ad->pedometerListener);
	if (ret != SENSOR_ERROR_NONE)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_create_listener() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}

	ret = sensor_listener_set_event_cb(ad->pedometerListener, 0, pedometer_cb, (void*) ad);
	if (ret != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_set_event_cb() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}
}

static void register_heartrate_listener(appdata_s *ad)
{
	sensor_h handle;
	int ret = sensor_get_default_sensor(SENSOR_HRM, &handle);
	if (ret != SENSOR_ERROR_NONE)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_get_default_sensor() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}

	ret = sensor_create_listener(handle, &ad->heartrateListener);
	if (ret != SENSOR_ERROR_NONE)
	{
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_create_listener() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}

	ret = sensor_listener_set_event_cb(ad->heartrateListener, 0, heartrate_cb, (void*) ad);
	if (ret != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_set_event_cb() error: %s", __FILE__, __LINE__, get_error_message(ret));
		return;
	}
}

static void start_listeners(appdata_s *ad)
{
	int ret;

	ret = sensor_listener_start(ad->pedometerListener);
	if (ret != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_start() error: %s", __FILE__, __LINE__, get_error_message(ret));
	}
}

static void stop_listeners(appdata_s *ad)
{
	int ret;

	ret = sensor_listener_stop(ad->pedometerListener);
	if (ret != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_stop() error: %s", __FILE__, __LINE__, get_error_message(ret));
	}

//	ret = sensor_listener_stop(ad->heartrateListener);
//	if (ret != SENSOR_ERROR_NONE) {
//		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_stop() error: %s", __FILE__, __LINE__, get_error_message(ret));
//	}
}

static void register_sensor_listeners(appdata_s *ad)
{
	register_pedometer_listener(ad);
	register_heartrate_listener(ad);
}

static Eina_Bool start_heartrate_measurement(void* data)
{
	appdata_s* ad = data;

	dlog_print(DLOG_INFO, LOG_TAG, "start_heartrate_measurement");

	// Reset data
	ad->countHrmMeasurements = ad->sumHrmMeasurements = 0;

	// Start listening to HRM
	int ret = sensor_listener_start(ad->heartrateListener);
	if (ret != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] sensor_listener_start() error: %s", __FILE__, __LINE__, get_error_message(ret));
	}

	return ECORE_CALLBACK_RENEW;
}

static bool app_create(int width, int height, void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */
	appdata_s *ad = data;

	// Create GUI
	create_base_gui(ad, width, height);

	// Init sensor listeners
	register_sensor_listeners(ad);

	// Start listenes
	start_listeners(ad);

	// Start with HRM
	start_heartrate_measurement(ad);

	// Check pulse every 30min
	ad->hrmTimer = ecore_timer_add(30. * 60.,
			start_heartrate_measurement,
			(void*) ad);

	return true;
}

static void
app_control(app_control_h app_control, void *data)
{
	/* Handle the launch request. */
}

static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible. */
	// stop_listeners(data);
}

static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible. */
	// start_listeners(data);
}

static void
app_terminate(void *data)
{
	/* Release all resources. */
	stop_listeners(data);
}

static void
app_time_tick(watch_time_h watch_time, void *data)
{
	// Called at each second while your app is visible. Update watch UI.
	appdata_s *ad = data;
	update_watch(ad, watch_time, 0);
}

static void
app_ambient_tick(watch_time_h watch_time, void *data)
{
	// Called at each minute while the device is in ambient mode. Update watch UI.
	appdata_s *ad = data;
	update_watch(ad, watch_time, 1);
}

static void
app_ambient_changed(bool ambient_mode, void *data)
{
	/* Update your watch UI to conform to the ambient mode */
}

static void
watch_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	app_event_get_language(event_info, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void
watch_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

int main(int argc, char *argv[])
{
	appdata_s ad = {0,};
	int ret = 0;

	watch_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;
	event_callback.time_tick = app_time_tick;
	event_callback.ambient_tick = app_ambient_tick;
	event_callback.ambient_changed = app_ambient_changed;

	watch_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
		APP_EVENT_LANGUAGE_CHANGED, watch_app_lang_changed, &ad);
	watch_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
		APP_EVENT_REGION_FORMAT_CHANGED, watch_app_region_changed, &ad);

	ret = watch_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "watch_app_main() is failed. err = %d", ret);
	}

	return ret;
}

