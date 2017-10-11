/* mbed Microcontroller Library
 * Copyright (c) 2017 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _M2M_OBJECT_HELPER_
#define _M2M_OBJECT_HELPER_

/** This class helps with constructing LWM2M objects for use with mbed
 * client or mbed cloud client.
 *
 * OVERVIEW
 *
 * LWM2M objects are made up of resources.  Resources may be readable
 * but static (so GET is allowed by the server), readable and
 * dynamic (GET is allowed and OBSERVABLE is also set), writable (PUT is
 * also allowed by the server) or executable (POST is allowed by the
 * server).  Resources are the leaves if your object is the tree.
 *
 * Resources and objects are identified by an ID string, which
 * will be a number, e.g. "1234".  The possible resources are standardised,
 * see: http://www.openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html
 * and https://github.com/IPSO-Alliance/pub/tree/master/reg for reference.
 *
 * Where an object contains two resources of the same ID (e.g. two timer
 * type resources) you will have two "instances" of the same resource.
 *
 * Standardised objects are also defined in the above references but
 * you are free to construct your own objects (using object IDs "32769"
 * and upwards).  Generally speaking, objects tend to consist of
 * readable resources or writable resources but not both; however this
 * is not a requirement.
 *
 * USAGE
 *
 * This class makes it easy to create LWM2M objects, using an object
 * definition structure and a few helper functions.
 *
 * To use it, simply subclass it and then, in your class, include a
 * const DefObject struct to define your LWM2M object.  Note that if
 * you're not yet using C11 the struct will have to be initialised in
 * a separate line outside your class, something like:
 *
 * const M2MObjectHelper::DefObject MyObject::_defObject =
 *   {0, "3312", 1,
 *       -1, "5850", "on/off", M2MResourceBase::BOOLEAN, false, M2MBase::GET_ALLOWED, NULL
 *   };
 *
 * You will then pass a pointer to this struct into the constructor of this
 * class. For example, if an object contains a single resource which is a
 * Boolean value, it might look something like this:
 *
 * MyObject::MyObject(bool initialValue)
 *          :M2MObjectHelper(&_defObject)
 * {
 *     makeObject();
 *     setResourceValue(initialValue, "5850");
 *     ...
 *
 * So your complete object definition might be as follows:
 *
 * class MyObject : public M2MObjectHelper {
 * public:
 *     MyObject(bool initialValue);
 *     ~MyObject();
 * protected:
 *     static const DefObject _defObject;
 * };
 *
 * ...and the implementation might be as follows:
 *
 * const M2MObjectHelper::DefObject MyObject::_defObject =
 *     {0, "3312", 1,
 *         -1, "5850", "on/off", M2MResourceBase::BOOLEAN, false, M2MBase::GET_ALLOWED, NULL
 *     };
 *
 * MyObject::MyObject(bool initialValue)
 *          :M2MObjectHelper(&_defObject)
 * {
 *     makeObject();
 *     setResourceValue(initialValue, "5850"));
 * }
 *
 * IocCtrlPowerControl::~IocCtrlPowerControl()
 * {
 * }
 *
 * CREATING OBJECTS WITH WRITABLE RESOURCES
 *
 * If your object includes a writable resource, i.e. one where PUT
 * is allowed, then you will need to do two things:
 *
 * - add a setCallback() function pointer, a callback capable of passing
 *   the newly written values from the LWM2M object back to your application,
 * - your class will include an objectUpdated() method which you will
 *   pass to this class in its constructor; it will be called when the
 *   server has updated a writable resource.
 *
 * For instance, if your object is a power on/off switch, your
 * setCallback() might take as a parameter the Boolean state of
 * the switch, e.g.
 *
 * MyObject::MyObject(Callback<void(bool)> setCallback,
 *                    bool initialValue)
 *          :M2MObjectHelper(&_defObject,
 *                           value_updated_callback(this, &MyObject::objectUpdated))
 * {
 *     _setCallback = setCallback;
 *     makeObject();
 *     setResourceValue(initialValue, "5850");
 *     ...
 *
 * The setCallback() function in your application code might look something
 * like this:
 *
 * void setPowerOnOff(bool powerIsOn)
 * {
 *     // Do something to switch the power on or off
 * }
 *
 * ...and the objectUpdated() method in your class might look something like
 * this:
 *
 * void MyObject::objectUpdated(const char *resourceName)
 * {
 *     bool onNotOff;
 *
 *     if (getResourceValue(&onNotOff, resourceName) {
 *         if (_setCallback) {
 *             _setCallback(onNotOff);
 *          }
 *     }
 * }
 *
 * For complete examples of the implementation of several different types of
 * LWM2M objects, take a look at the files ioc_m2m.h and ioc_m2m.cpp in
 * this repo:
 *
 * https://github.com/u-blox/ioc-client
 *
 * CREATING OBJECTS WITH OBSERVABLE (i.e. changing) RESOURCES
 *
 * If your object includes one or more observable resources, i.e. ones
 * which can change from their initial value, then you will need to do three
 * things:
 *
 * - add a getCallback() function pointer, a callback capable of retrieving
 *   the new values from your application code so that the values in
 *   the LWM2M object can be updated,
 * - your class will need to include an updateObservableResources() method
 *   that maps the values returned in getCallback() to the LWM2M object,
 * - your application will then arrange for the updateObservableResources()
 *   of your class to be called either periodically or when a value has
 *   changed.  This class provides a default implementation of
 *   updateObservableResources() (which does nothing) so it is always safe
 *   to call this method on any object.
 *
 * For instance, if your object were a temperature sensor your getCallback()
 * would take as a parameter a pointer to a structure of your creation, e.g.:
 *
 * MyObject::MyObject(Callback<bool(Temperature *)> getCallback,
 *                    int64_t aFixedValueThing)
 *          :M2MObjectHelper(&_defObject)
 * {
 *     _getCallback = getCallback;
 *     makeObject();
 *     setResourceValue(aFixedValueThing, "5603");
 *     ...
 *
 * ...your getCallback() function in your application code might look
 * something like:
 *
 * bool getTemperatureData(MyObject::Temperature *data)
 * {
 *     data->temperature = <some way of getting the temperature value>;
 *     data->minTemperature = <some way of getting the min temperature value>;
 *     data->maxTemperature = <some way of getting the max temperature value>;
 *
 *     return true;
 * }
 *
 * ...and your updateObservableResources() method would link the
 * two together like this:
 *
 * void MyObject::updateObservableResources()
 * {
 *     Temperature data;
 *
 *     if (_getCallback) {
 *         if (_getCallback(&data)) {
 *             setResourceValue(data.temperature, "5700");
 *             setResourceValue(data.minTemperature, "5601");
 *             setResourceValue(data.maxTemperature, "5602");
 *         }
 *     }
 * }
 *
 * CREATING OBJECTS WITH EXECUTABLE RESOURCES
 *
 * If your object includes an executable resource, you will need to do
 * three things:
 *
 * - add a pointer to a callback function which executes the action
 *   to your classes' constructor,
 * - your class will need to include an executeFunction() (or something
 *   like that) which calls the callback,
 * - you will need to attach the callback to the resource ID in the
 *   constructor of your class.
 *
 * For instance, if your temperature sensor has a resetMinMax function,
 * your constructor might look like this:
 *
 *  MyObject::MyObject(Callback<bool(Temperature *)> getCallback)
 *                     Callback<void(void)> resetMinMaxCallback,
 *                    int64_t aFixedValueThing)
 *           :M2MObjectHelper(&_defObject)
 * {
 *     _getCallback = getCallback;
 *     _resetMinMaxCallback = resetMinMaxCallback;
 *     makeObject();
 *     setResourceValue(aFixedValueThing, "5603");
 *     ...
 *
 * ...and your class would include a method that looks something like
 * this:
 *
 * void MyObject::executeFunction (void *parameter)
 * {
 *     if (_resetMinMaxCallback) {
 *         _resetMinMaxCallback();
 *     }
 * }
 *
 * ...then in the body of your classes' constructor you would link the
 * two something like this:
 *
 * if (resetMinMaxCallback) {
 *       setExecuteCallback(execute_callback(this, &MyObject::executeFunction),
 *                         "5605"));
 * }
 *
 * CREATING MULTIPLE OBJECTS OF THE SAME TYPE
 *
 * If you need to create multiple objects with the same ID string, e.g.
 * an indoor and an outdoor temperature sensor, both of which will
 * have the ID "3303", you will need to define separate classes for
 * each one with their unique instance IDs (e.g. 0 and 1) included in the
 * DefObject structure.  You will then need to add a pointer to M2MObject
 * to the constructor of each of your object classes and pass that pointer
 * to this class.
 *
 * For instance, the constructors might look as follows:
 *
 * TemperatureOutdoor::TemperatureOutdoor(Callback<bool(Temperature *)> getCallback,
 *                                        M2MObject *object)
 *                    :M2MObjectHelper(&_defObject, NULL, object)
 * {
 *     ...
 * }
 *
 * TemperatureIndoor::TemperatureIndoor(Callback<bool(Temperature *)> getCallback,
 *                                      M2MObject *object)
 *                   :M2MObjectHelper(&_defObject, NULL, object)
 * {
 *     ...
 * }
 *
 * Then, in your application code, you would instantiate the two temperature
 * objects as follows:
 *
 * TemperatureOutdoor *outdoor = new TemperatureOutdoor(true,
 *                                                      getTemperatureDataOutdoor,
 *                                                      NULL);
 * M2MObject *objectOutdoor = getObject(outdoor);
 * TemperatureIndoor *indoor = new TemperatureIndoor(true,
 *                                                   getTemperatureDataIndoor,
 *                                                   objectOutdoor);
 *
 * CLEARING UP
 *
 * When clearing objects up, always delete them BEFORE mbed client/cloud client
 * itself is deleted (since their destructors do things inside mbed client/cloud
 * client).
 */
class M2MObjectHelper {
public:

    /** Destructor.
     */
    virtual ~M2MObjectHelper();

    /** Update this objects' resources.
     * Derived classes should implement
     * this function (and make a call to
     * their setCallback() function) if they
     * have observable resources which need
     * to be updated from somewhere (e.g.
     * the temperature has changed, so the
     * resource value here has to be updated
     * to match).
     */
    virtual void updateObservableResources();

    /** Return this object.
     *
     * @return pointer to this object.
     */
    M2MObject *getObject();

protected:

    /** The maximum length of an object
     * name or resource name.
     */
#   ifndef MAX_OBJECT_RESOURCE_NAME_LENGTH
#   define MAX_OBJECT_RESOURCE_NAME_LENGTH 8
#   endif

    /** The maximum length of the string
     * representation of a resource type.
     */
#   ifndef MAX_RESOURCE_TYPE_LENGTH
#   define MAX_RESOURCE_TYPE_LENGTH 20
#   endif

    /** The maximum number of resources
     * an object can have.
     */
#   ifndef MAX_NUM_RESOURCES
#   define MAX_NUM_RESOURCES 8
#   endif

    /** Structure to represent a resource.
     */
    typedef struct {
        int instance; ///< use -1 if there is only a single instance, as then
                      /// the instance field is not required.
        const char name[MAX_OBJECT_RESOURCE_NAME_LENGTH]; ///< the name, e.g. "3303".
        const char typeString[MAX_RESOURCE_TYPE_LENGTH]; ///< the type. e.g. "on/off".
        M2MResourceBase::ResourceType type;
        bool observable; ///< true if the object is observable, otherwise false.
        M2MBase::Operation operation;
        const char * format; ///< format string, can be user to present
                             /// a nicely formatted value if type is FLOAT.
    } DefResource;

    /** Structure to represent an object.
     */
    typedef struct {
        int instance;
        char name[MAX_OBJECT_RESOURCE_NAME_LENGTH];
        int numResources;
        DefResource resources[MAX_NUM_RESOURCES];
    } DefObject;

    /** Constructor.
     *
     * @param defObject              the definition of the LWM2M object.
     * @param valueUpdatedCallback   callback to be called if any
     *                               resource in this object is written
     *                               to; the callback will receive the
     *                               resource number (though not the instance
     *                               number, the M2MClient code doesn't seem
     *                               to do that) as a string so that
     *                               finer-grained action can be performed
     *                               if required.
     * @param object                 if this is the second (or more) instance
     *                               of the same object type then a pointer to
     *                               the first object of this type that was
     *                               created should be passed in here.
     * @param debugOn                true to switch debug prints on,
     *                               otherwise false.
     */
    M2MObjectHelper(const DefObject *defObject,
                    value_updated_callback valueUpdatedCallback = NULL,
                    M2MObject *object = NULL,
                    bool debugOn = false);

    /** Create an object as defined in the defObject
     * structure passed in in the constructor. This
     * must be called before any of the other
     * functions can be called.
     *
     * @return  true if successful, otherwise false.
     */
    bool makeObject();

    /** Set the execute callback (for an executable resource).
     *
     * @param callback the callback.
     * @return         true if successful, otherwise false.
     */
    bool setExecuteCallback(execute_callback callback,
                            const char *resourceNumber);

    /** Set the value of a given resource in an object.
     *
     * @param value            the value of the resource to set.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(int64_t value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Set the value of a given resource in an object.
     *
     * @param value            the value of the resource to set.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(float value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Set the value of a given resource in an object.
     *
     * @param value            the value of the resource to set.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(bool value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Set the value of a given resource in an object.
     *
     * @param value            the value of the resource to set.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(const char *value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Set the value of a given resource in an object.
     *
     * @param value            the value of the resource to set.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(String value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(int64_t *value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(float *value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(bool *value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.
     * @param len              the length of the buffer pointed
     *                         to by value.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(char *value,
                          unsigned int  len,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(String *value,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** True if debug is on, otherwise false.
     */
    bool _debugOn;

    /** Need this for the types.
     */
    friend class M2MBase;

private:

    /** Set the value of a given resource in an object.
     *
     * @param value            pointer to the value of the
     *                         resource to set. If the
     *                         resource is of type STRING,
     *                         value should be a pointer to
     *                         String, if the resource
     *                         is of type INTEGER, value
     *                         should be a pointer to int64_t,
     *                         if the resource is of type FLOAT,
     *                         value should be a pointer to
     *                         float and if the resource is of
     *                         type BOOLEAN the value should
     *                         be a pointer to bool.
     * @param resourceType     the resource type.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @param format           the format string, only relevant
     *                         if type is String.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool setResourceValue(const void *value,
                          M2MResourceBase::ResourceType type,
                          const char *resourceNumber,
                          int wantedInstance = -1,
                          const char *format = NULL);

    /** Get the value of a given resource in an object.
     *
     * @param value            pointer to a place to put
     *                         the resource value.  If
     *                         the resource is of type STRING,
     *                         value should be a pointer to
     *                         String (NOT char *), if the
     *                         resource is of type INTEGER,
     *                         value should be a pointer to
     *                         int64_t, if the resource is of
     *                         type FLOAT, value should be a
     *                         pointer to float and if the
     *                         resource is of type BOOLEAN
     *                         the value should be a pointer
     *                         to bool.
     * @param resourceType     the resource type.
     * @param resourceNumber   the number of the resource whose
     *                         value is to be set.
     * @param wantedInstance   the resource instance if there
     *                         is more than one.
     * @return                 true if successful, otherwise
     *                         false.
     */
    bool getResourceValue(void *value,
                          M2MResourceBase::ResourceType type,
                          const char *resourceNumber,
                          int wantedInstance = -1);

    /** A pointer to the definition for this object.
     */
    const DefObject *_defObject;

    /** A pointer to the LWM2M object.
     */
    M2MObject *_object;

    /** The value updated callback, may be NULL.  This should be
     * set if the object includes a writable resource and you
     * want to know when it has been written-to by the server
     * (so that you can update the local values in your code
     * as appropriate).
     */
    value_updated_callback _valueUpdatedCallback;
};

#endif // _M2M_OBJECT_HELPER_

// End of file
