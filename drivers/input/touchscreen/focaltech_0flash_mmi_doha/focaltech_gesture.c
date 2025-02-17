/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_gestrue.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"
#if FTS_GESTURE_EN
/******************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define KEY_GESTURE_U                           KEY_U
#define KEY_GESTURE_UP                          KEY_UP
#define KEY_GESTURE_DOWN                        KEY_DOWN
#define KEY_GESTURE_LEFT                        KEY_LEFT
#define KEY_GESTURE_RIGHT                       KEY_RIGHT
#define KEY_GESTURE_O                           KEY_O
#define KEY_GESTURE_E                           KEY_E
#define KEY_GESTURE_M                           KEY_M
#define KEY_GESTURE_L                           KEY_L
#define KEY_GESTURE_W                           KEY_W
#define KEY_GESTURE_S                           KEY_S
#define KEY_GESTURE_V                           KEY_V
#define KEY_GESTURE_C                           KEY_C
#define KEY_GESTURE_Z                           KEY_Z

#define GESTURE_LEFT                            0x20
#define GESTURE_RIGHT                           0x21
#define GESTURE_UP                              0x22
#define GESTURE_DOWN                            0x23
#define GESTURE_DOUBLECLICK                     0x24
#define GESTURE_O                               0x30
#define GESTURE_W                               0x31
#define GESTURE_M                               0x32
#define GESTURE_E                               0x33
#define GESTURE_L                               0x44
#define GESTURE_S                               0x46
#define GESTURE_V                               0x54
#define GESTURE_Z                               0x41
#define GESTURE_C                               0x34

#ifdef FOCALTECH_SENSOR_EN
#define REPORT_MAX_COUNT 10000
#endif

/* Double tap detection resources */
#define FOCALTECH_DT2W_ENABLE	1

#if FOCALTECH_DT2W_ENABLE
#define DT2W_FEATHER	150
#define DT2W_TIME	500

static unsigned long long tap_time_pre;
static int touch_nr, x_pre, y_pre;
#endif /* FOCALTECH_DT2W_ENABLE */

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/*
* gesture_id    - mean which gesture is recognised
* point_num     - points number of this gesture
* coordinate_x  - All gesture point x coordinate
* coordinate_y  - All gesture point y coordinate
* mode          - gesture enable/disable, need enable by host
*               - 1:enable gesture function(default)  0:disable
* active        - gesture work flag,
*                 always set 1 when suspend, set 0 when resume
*/
struct fts_gesture_st {
    u8 gesture_id;
    u8 point_num;
    u16 coordinate_x[FTS_GESTURE_POINTS_MAX];
    u16 coordinate_y[FTS_GESTURE_POINTS_MAX];
    u8 mode;
    u8 active;
};

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct fts_gesture_st fts_gesture_data;
#ifdef FOCALTECH_SENSOR_EN
#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock gesture_wakelock;
#else
static struct wakeup_source gesture_wakelock;
#endif
static struct sensors_classdev __maybe_unused sensors_touch_cdev = {
    .name = "dt-gesture",
    .vendor = "Focaltech",
    .version = 1,
    .type = SENSOR_TYPE_MOTO_DOUBLE_TAP,
    .max_range = "5.0",
    .resolution = "5.0",
    .sensor_power = "1",
    .min_delay = 0,
    .max_delay = 0,
    /* WAKE_UP & SPECIAL_REPORT */
    .flags = 1 | 6,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 200,
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
};
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static ssize_t fts_gesture_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct input_dev *input_dev = fts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_GESTURE_EN, &val);
    count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
                     fts_gesture_data.mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input_dev = fts_data->input_dev;
    mutex_lock(&input_dev->mutex);
    if (FTS_SYSFS_ECHO_ON(buf)) {
        FTS_DEBUG("enable gesture");
        fts_gesture_data.mode = ENABLE;
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        FTS_DEBUG("disable gesture");
        fts_gesture_data.mode = DISABLE;
    }
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    int i = 0;
    struct input_dev *input_dev = fts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    mutex_lock(&input_dev->mutex);
    count = snprintf(buf, PAGE_SIZE, "Gesture ID:%d\n", gesture->gesture_id);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture PointNum:%d\n",
                      gesture->point_num);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture Points Buffer:\n");

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        count += snprintf(buf + count, PAGE_SIZE, "%3d(%4d,%4d) ", i,
                          gesture->coordinate_x[i], gesture->coordinate_y[i]);
        if ((i + 1) % 4 == 0)
            count += snprintf(buf + count, PAGE_SIZE, "\n");
    }
    count += snprintf(buf + count, PAGE_SIZE, "\n");
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}


/* sysfs gesture node
 *   read example: cat  fts_gesture_mode       ---read gesture mode
 *   write example:echo 1 > fts_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR(fts_gesture_mode, S_IRUGO | S_IWUSR, fts_gesture_show,
                   fts_gesture_store);
/*
 *   read example: cat fts_gesture_buf        --- read gesture buf
 */
static DEVICE_ATTR(fts_gesture_buf, S_IRUGO | S_IWUSR,
                   fts_gesture_buf_show, fts_gesture_buf_store);

static struct attribute *fts_gesture_mode_attrs[] = {
    &dev_attr_fts_gesture_mode.attr,
    &dev_attr_fts_gesture_buf.attr,
    NULL,
};

static struct attribute_group fts_gesture_group = {
    .attrs = fts_gesture_mode_attrs,
};

int fts_create_gesture_sysfs(struct device *dev)
{
    int ret = 0;

    ret = sysfs_create_group(&dev->kobj, &fts_gesture_group);
    if (ret) {
        FTS_ERROR("gesture sys node create fail");
        sysfs_remove_group(&dev->kobj, &fts_gesture_group);
        return ret;
    }

    return 0;
}

static void fts_gesture_report(struct input_dev *input_dev, int gesture_id)
{
    int gesture;
#ifdef FOCALTECH_SENSOR_EN
    static int report_cnt = 0;
#endif

    FTS_DEBUG("gesture_id:0x%x", gesture_id);
    switch (gesture_id) {
    case GESTURE_LEFT:
        gesture = KEY_GESTURE_LEFT;
        break;
    case GESTURE_RIGHT:
        gesture = KEY_GESTURE_RIGHT;
        break;
    case GESTURE_UP:
        gesture = KEY_GESTURE_UP;
        break;
    case GESTURE_DOWN:
        gesture = KEY_GESTURE_DOWN;
        break;
    case GESTURE_DOUBLECLICK:
        gesture = KEY_GESTURE_U;
        break;
    case GESTURE_O:
        gesture = KEY_GESTURE_O;
        break;
    case GESTURE_W:
        gesture = KEY_GESTURE_W;
        break;
    case GESTURE_M:
        gesture = KEY_GESTURE_M;
        break;
    case GESTURE_E:
        gesture = KEY_GESTURE_E;
        break;
    case GESTURE_L:
        gesture = KEY_GESTURE_L;
        break;
    case GESTURE_S:
        gesture = KEY_GESTURE_S;
        break;
    case GESTURE_V:
        gesture = KEY_GESTURE_V;
        break;
    case GESTURE_Z:
        gesture = KEY_GESTURE_Z;
        break;
    case  GESTURE_C:
        gesture = KEY_GESTURE_C;
        break;
    default:
        gesture = -1;
        break;
    }
    /* report event key */
    if (gesture != -1) {
        FTS_DEBUG("Gesture Code=%d", gesture);
#ifdef FOCALTECH_SENSOR_EN
        if (!(fts_data->wakeable && fts_data->should_enable_gesture)) {
            FTS_INFO("Gesture got but wakeable not set. Skip this gesture.");
            return;
        }
        if (fts_data->pdata->report_gesture_key) {
#if FOCALTECH_DT2W_ENABLE
            input_report_key(fts_data->sensor_pdata->input_sensor_dev, KEY_WAKEUP, 1);
#else
            input_report_key(fts_data->sensor_pdata->input_sensor_dev, KEY_F1, 1);
#endif /* FOCALTECH_DT2W_ENABLE */
            input_sync(fts_data->sensor_pdata->input_sensor_dev);
#if FOCALTECH_DT2W_ENABLE
            input_report_key(fts_data->sensor_pdata->input_sensor_dev, KEY_WAKEUP, 0);
#else
            input_report_key(fts_data->sensor_pdata->input_sensor_dev, KEY_F1, 0);
#endif /* FOCALTECH_DT2W_ENABLE */
            input_sync(fts_data->sensor_pdata->input_sensor_dev);
            ++report_cnt;
        } else {
            input_report_abs(fts_data->sensor_pdata->input_sensor_dev,
                            ABS_DISTANCE, ++report_cnt);
            input_sync(fts_data->sensor_pdata->input_sensor_dev);
        }
        FTS_INFO("input report: %d", report_cnt);
        if (report_cnt >= REPORT_MAX_COUNT)
            report_cnt = 0;

#ifdef CONFIG_HAS_WAKELOCK
        wake_lock_timeout(&gesture_wakelock, msecs_to_jiffies(5000));
#else
        __pm_wakeup_event(&gesture_wakelock, 5000);
#endif
#else
        input_report_key(input_dev, gesture, 1);
        input_sync(input_dev);
        input_report_key(input_dev, gesture, 0);
        input_sync(input_dev);
#endif
    }
}

#ifdef FOCALTECH_SENSOR_EN
static int fts_sensor_set_enable(struct sensors_classdev *sensors_cdev,
    unsigned int enable)
{
    FTS_INFO("Gesture set enable %d!", enable);
    mutex_lock(&fts_data->state_mutex);
    if (enable == 1)
        fts_data->should_enable_gesture = true;
    else if (enable == 0)
        fts_data->should_enable_gesture = false;
    else
        FTS_INFO("unknown enable symbol\n");

    mutex_unlock(&fts_data->state_mutex);
    return 0;
}

static int fts_sensor_init(struct fts_ts_data *data)
{
    struct focaltech_sensor_platform_data *sensor_pdata;
    struct input_dev *sensor_input_dev;
    int err;

    sensor_input_dev = input_allocate_device();
    if (!sensor_input_dev) {
        FTS_ERROR("Failed to allocate device");
        goto exit;
    }

    sensor_pdata = devm_kzalloc(&sensor_input_dev->dev,
                                sizeof(struct focaltech_sensor_platform_data),
                                GFP_KERNEL);
    if (!sensor_pdata) {
        FTS_ERROR("Failed to allocate memory");
        goto free_sensor_pdata;
    }
    data->sensor_pdata = sensor_pdata;

    if (data->pdata->report_gesture_key) {
        __set_bit(EV_KEY, sensor_input_dev->evbit);
#if FOCALTECH_DT2W_ENABLE
        __set_bit(KEY_WAKEUP, sensor_input_dev->keybit);
#else
        __set_bit(KEY_F1, sensor_input_dev->keybit);
#endif /* FOCALTECH_DT2W_ENABLE */
    }
    else {
        __set_bit(EV_ABS, sensor_input_dev->evbit);
        input_set_abs_params(sensor_input_dev, ABS_DISTANCE,
                                0, REPORT_MAX_COUNT, 0, 0);
    }
    __set_bit(EV_SYN, sensor_input_dev->evbit);

    sensor_input_dev->name = "double-tap";
    data->sensor_pdata->input_sensor_dev = sensor_input_dev;

    err = input_register_device(sensor_input_dev);
    if (err) {
        FTS_ERROR("Unable to register device, err=%d", err);
        goto free_sensor_input_dev;
    }

    sensor_pdata->ps_cdev = sensors_touch_cdev;
    sensor_pdata->ps_cdev.sensors_enable = fts_sensor_set_enable;
    sensor_pdata->data = data;

    err = sensors_classdev_register(&sensor_input_dev->dev,
                                    &sensor_pdata->ps_cdev);
    if (err)
        goto unregister_sensor_input_device;

    return 0;

unregister_sensor_input_device:
    input_unregister_device(data->sensor_pdata->input_sensor_dev);
free_sensor_input_dev:
    input_free_device(data->sensor_pdata->input_sensor_dev);
free_sensor_pdata:
    devm_kfree(&sensor_input_dev->dev, sensor_pdata);
    data->sensor_pdata = NULL;
exit:
    return 1;
}

int fts_sensor_remove(struct fts_ts_data *data)
{
    sensors_classdev_unregister(&data->sensor_pdata->ps_cdev);
    input_unregister_device(data->sensor_pdata->input_sensor_dev);
    devm_kfree(&data->sensor_pdata->input_sensor_dev->dev,
               data->sensor_pdata);
    data->sensor_pdata = NULL;
    data->wakeable = false;
    data->should_enable_gesture = false;
    return 0;
}
#endif

#if FOCALTECH_DT2W_ENABLE
/* Doubletap2wake */
static void doubletap2wake_reset(void)
{
	touch_nr = 0;
	tap_time_pre = 0;
	x_pre = 0;
	y_pre = 0;
}

static unsigned int calc_feather(int coord, int prev_coord)
{
	int calc_coord = 0;

	calc_coord = coord-prev_coord;
	if (calc_coord < 0)
		calc_coord = calc_coord * (-1);
	return calc_coord;
}

static void new_touch(int x, int y)
{
	tap_time_pre = ktime_to_ms(ktime_get());
	x_pre = x;
	y_pre = y;
	touch_nr++;
}

static bool detect_doubletap2wake(int x, int y)
{
	if (touch_nr == 0) {
		new_touch(x, y);
	} else if (touch_nr == 1) {
		if ((calc_feather(x, x_pre) < DT2W_FEATHER) &&
			(calc_feather(y, y_pre) < DT2W_FEATHER) &&
			((ktime_to_ms(ktime_get())-tap_time_pre) < DT2W_TIME))
			touch_nr++;
		else {
			doubletap2wake_reset();
			new_touch(x, y);
		}
	} else {
		doubletap2wake_reset();
		new_touch(x, y);
	}
	if (touch_nr > 1) {
		doubletap2wake_reset();
		return true;
	}
	return false;
}
#endif /* FOCALTECH_DT2W_ENABLE */

/*****************************************************************************
* Name: fts_gesture_readdata
* Brief: Read information about gesture: enable flag/gesture points..., if ges-
*        ture enable, save gesture points' information, and report to OS.
*        It will be called this function every intrrupt when FTS_GESTURE_EN = 1
*
*        gesture data length: 1(enable) + 1(reserve) + 2(header) + 6 * 4
* Input: ts_data - global struct data
*        data    - gesture data buffer if non-flash, else NULL
* Output:
* Return: 0 - read gesture data successfully, the report data is gesture data
*         1 - tp not in suspend/gesture not enable in TP FW
*         -Exx - error
*****************************************************************************/
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data)
{
    int ret = 0;
    int i = 0;
    int index = 0;
    u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
    struct input_dev *input_dev = ts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    if (!ts_data->suspended || (DISABLE == gesture->mode)) {
        return 1;
    }

    if (!data) {
        FTS_ERROR("gesture data buffer is null");
        ret = -EINVAL;
        return ret;
    }

    memcpy(buf, data, FTS_GESTURE_DATA_LEN);
    if (buf[0] != ENABLE) {
        FTS_DEBUG("gesture not enable in fw, don't process gesture");
        return 1;
    }


    /* init variable before read gesture point */
    memset(gesture->coordinate_x, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    memset(gesture->coordinate_y, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    gesture->gesture_id = buf[2];
    gesture->point_num = buf[3];
    FTS_DEBUG("gesture_id=%d, point_num=%d",
              gesture->gesture_id, gesture->point_num);

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        index = 4 * i + 4;
        gesture->coordinate_x[i] = (u16)(((buf[0 + index] & 0x0F) << 8)
                                         + buf[1 + index]);
        gesture->coordinate_y[i] = (u16)(((buf[2 + index] & 0x0F) << 8)
                                         + buf[3 + index]);
    }

    /* report gesture to OS */
#if FOCALTECH_DT2W_ENABLE
	if (detect_doubletap2wake(gesture->coordinate_x[0], gesture->coordinate_y[0]))
#endif /* FOCALTECH_DT2W_ENABLE */
		fts_gesture_report(input_dev, gesture->gesture_id);

    return 0;
}

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
    if ((ENABLE == fts_gesture_data.mode) && (ENABLE == fts_gesture_data.active)) {
        FTS_DEBUG("gesture recovery...");
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
    }
}

int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int i = 0;
    u8 state = 0xFF;

    FTS_INFO("gesture suspend...");
    /* gesture not enable, return immediately */
    if (fts_gesture_data.mode == DISABLE) {
        FTS_DEBUG("gesture is disabled");
        return -EINVAL;
    }

    for (i = 0; i < 5; i++) {
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
        msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state == ENABLE)
            break;
    }

    if (i >= 5) {
        FTS_ERROR("Enter into gesture(suspend) fail");
        fts_gesture_data.active = DISABLE;
        return -EIO;
    }

    ret = enable_irq_wake(ts_data->irq);
    if (ret) {
        FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);
    }

    fts_gesture_data.active = ENABLE;
    FTS_INFO("Enter into gesture(suspend) successfully!");
    return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int i = 0;
    u8 state = 0xFF;

    FTS_INFO("gesture resume...");
    /* gesture not enable, return immediately */
    if (fts_gesture_data.mode == DISABLE) {
        FTS_DEBUG("gesture is disabled");
        return -EINVAL;
    }

    if (fts_gesture_data.active == DISABLE) {
        FTS_DEBUG("gesture active is disable, return immediately");
        return -EINVAL;
    }

    fts_gesture_data.active = DISABLE;
    for (i = 0; i < 5; i++) {
        fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
        msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state == DISABLE)
            break;
    }

    if (i >= 5) {
        FTS_ERROR("exit gesture(resume) fail");
        return -EIO;
    }

    ret = disable_irq_wake(ts_data->irq);
    if (ret) {
        FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);
    }

    FTS_INFO("resume from gesture successfully");
    return 0;
}

int fts_gesture_init(struct fts_ts_data *ts_data)
{
    struct input_dev *input_dev = ts_data->input_dev;
#ifdef FOCALTECH_SENSOR_EN
    static bool initialized_sensor;
#endif

    FTS_FUNC_ENTER();
    input_set_capability(input_dev, EV_KEY, KEY_POWER);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);

    __set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
    __set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
    __set_bit(KEY_GESTURE_UP, input_dev->keybit);
    __set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
    __set_bit(KEY_GESTURE_U, input_dev->keybit);
    __set_bit(KEY_GESTURE_O, input_dev->keybit);
    __set_bit(KEY_GESTURE_E, input_dev->keybit);
    __set_bit(KEY_GESTURE_M, input_dev->keybit);
    __set_bit(KEY_GESTURE_W, input_dev->keybit);
    __set_bit(KEY_GESTURE_L, input_dev->keybit);
    __set_bit(KEY_GESTURE_S, input_dev->keybit);
    __set_bit(KEY_GESTURE_V, input_dev->keybit);
    __set_bit(KEY_GESTURE_C, input_dev->keybit);
    __set_bit(KEY_GESTURE_Z, input_dev->keybit);

#ifdef FOCALTECH_SENSOR_EN
    if (!initialized_sensor) {
#ifdef CONFIG_HAS_WAKELOCK
        wake_lock_init(&gesture_wakelock, WAKE_LOCK_SUSPEND, "poll-wake-lock");
#else
        wakeup_source_init(&gesture_wakelock, "ets_wake_lock");
#endif
        if (!fts_sensor_init(ts_data))
            initialized_sensor = true;
    }
#endif

    fts_create_gesture_sysfs(ts_data->dev);

    memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
    fts_gesture_data.mode = ENABLE;
    fts_gesture_data.active = DISABLE;

    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_exit(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
    FTS_FUNC_EXIT();
    return 0;
}
#endif
