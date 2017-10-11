Introduction
============
This class helps with constructing LWM2M objects for use with Mbed Client or Mbed Cloud Client.

LWM2M objects are made up of resources.  Resources may be readable but static (so GET is allowed by the server), readable and dynamic (GET is allowed by OBSERVABLE is also set), writable (PUT is also allowed by the server) or executable (POST is allowed by the server).  Resources are the leaves if your object is the tree.

Resources and objects are identified by an ID string, which will be a number, e.g. "1234".  The possible resources are standardised, see: http://www.openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html and https://github.com/IPSO-Alliance/pub/tree/master/reg for reference.

Where an object contains two resources of the same ID (e.g. two timer type resources) you will have two "instances" of the same resource.

Standardised objects are also defined in the above references but you are free to construct your own objects (using object IDs "32769" and upwards).  Generally speaking, objects tend to consist of readable resources or writable resources but not both; however this is not a requirement.

Usage
=====
This class makes it easy to create LWM2M objects, using an object definition structure and a few helper functions.

To use it, simply subclass it and then, in your class, include a `const DefObject` struct to define your LWM2M object.  Note that if you're not yet using C11 the struct will have to be initialised in a separate line outside your class, something like:

```
const M2MObjectHelper::DefObject MyObject::_defObject =
  {0, "3312", 1,
      -1, "5850", "on/off", M2MResourceBase::BOOLEAN, false, M2MBase::GET_PUT_ALLOWED, NULL
  };
```

You will then pass a pointer to this struct into the constructor of this class. For example, if an object contains a single resource which is a Boolean value, it might look something like this:

```
MyObject::MyObject(bool initialValue)
         :M2MObjectHelper(&_defObject)
{
    makeObject();
    setResourceValue(initialValue, "5850");
    ...
```

So your complete object definition might look something like this:

```
class MyObject : public M2MObjectHelper {
public:

    MyObject(bool initialValue);
    ~MyObject();
protected:
    static const DefObject _defObject;
};
```

...and the implementation would be:

```
const M2MObjectHelper::DefObject MyObject::_defObject =
    {0, "3312", 1,
        -1, "5850", "on/off", M2MResourceBase::BOOLEAN, false, M2MBase::GET_PUT_ALLOWED, NULL
    };

MyObject::MyObject(bool initialValue)
         :M2MObjectHelper(&_defObject)
{
    makeObject();
    setResourceValue(initialValue, "5850"));
}

MyObject::~MyObject()
{
}
```

For complete examples of the implementation of several different types of LWM2M objects, take a look at the files `ioc_m2m.h` and `ioc_m2m.cpp` in this repo:

https://github.com/u-blox/ioc-client

Creating Objects With Writable Resources
----------------------------------------
If your object includes a writable resource, i.e. one where PUT is allowed, then you will need to do two things:

 - add a `setCallback()` function pointer, a callback capable of passing the newly written values from the LWM2M object back to your application,
 - your class will include an `objectUpdated()` method which you will pass to this class in its constructor; it will be called when the server has updated a writable resource.

For instance, if your object is a power on/off switch, your `setCallback()` might take as a parameter the Boolean state of the switch, e.g.

```
MyObject::MyObject(Callback<void(bool)> setCallback,
                   bool initialValue)
         :M2MObjectHelper(&_defObject,
                          value_updated_callback(this, &MyObject::objectUpdated))
{
    _setCallback = setCallback;
    makeObject();
    setResourceValue(initialValue, "5850");
    ...
```

The `setCallback()` function in your application might look something like this:

```
void setPowerOnOff(bool powerIsOn)
{
    // Do something to switch the power on or off
}
```

...and the `objectUpdated()` method in your class might look something like this:

```
void MyObject::objectUpdated(const char *resourceName)
{
    bool onNotOff;

    if (getResourceValue(&onNotOff, resourceName) {
        if (_setCallback) {
            _setCallback(onNotOff);
         }
    }
}
```

Creating Objects With Observable (i.e. Changing) Resources
----------------------------------------------------------
If your object includes one or more observable resources, i.e. ones which can change from their initial value, then you will need to do three things:

- add a `getCallback()` function pointer, a callback capable of retrieving the new values from your application code so that the values in the LWM2M object can be updated,
- your class will need to include an `updateObservableResources()` method that maps the values returned in getCallback() to the LWM2M object,
- your application will then arrange for the `updateObservableResources()` of your class to be called either periodically or when a value has changed.  This class provides a default implementation of `updateObservableResources()` (which does nothing) so it is always safe to call this method on any LWM2M object.

For instance, if your object were a temperature sensor your `getCallback()` would take as a parameter a pointer to a structure of your creation, e.g.:

```
MyObject::MyObject(Callback<bool(Temperature *)> getCallback,
                   int64_t aFixedValueThing)
         :M2MObjectHelper(&_defObject)
{
    _getCallback = getCallback;
    makeObject();
    setResourceValue(aFixedValueThing, "5603");
    ...
```

...your `getCallback()` function might look something like:

```
bool getTemperatureData(MyObject::Temperature *data)
{
    data->temperature = <some way of getting the temperature value>;
    data->minTemperature = <some way of getting the min temperature value>;
    data->maxTemperature = <some way of getting the max temperature value>;
    return true;
}
```

...and your `updateObservableResources()` method would link the two together like this:

```
void MyObject::updateObservableResources()
{
    Temperature data;

    if (_getCallback) {
        if (_getCallback(&data)) {
            setResourceValue(data.temperature, "5700");
            setResourceValue(data.minTemperature, "5601");
            setResourceValue(data.maxTemperature, "5602");
        }
    }
}
```

Creating Objects With Executable Resources
------------------------------------------
If your object includes an executable resource, you will need to do three things:

- add a pointer to a callback function which executes the action to your classes' constructor,
- your class will need to include an `executeFunction()` (or something like that) which calls the callback,
- you will need to attach the callback to the resource ID in the constructor of your class.

For instance, if your temperature sensor has a `resetMinMax` function, your constructor might look like this:

```
MyObject::MyObject(Callback<bool(Temperature *)> getCallback)
                   Callback<void(void)> resetMinMaxCallback,
                   int64_t aFixedValueThing)
         :M2MObjectHelper(&_defObject)
{
    _getCallback = getCallback;
    _resetMinMaxCallback = resetMinMaxCallback;
    makeObject();
    setResourceValue(aFixedValueThing, "5603");
    ...
```

...and your class might include a method that looks something like this:

```
void MyObject::executeFunction (void *parameter)
{
    if (_resetMinMaxCallback) {
        _resetMinMaxCallback();
    }
}
```

...then in the body of your classes' constructor you would link the two something like this:

```
if (resetMinMaxCallback) {
    setExecuteCallback(execute_callback(this, &MyObject::executeFunction), "5605"));
}
```

Creating Multiple Objects Of The Same Type
------------------------------------------
If you need to create multiple objects with the same ID string, e.g. an indoor and an outdoor temperature sensor, both of which will have the ID "3303", you will need to define separate classes for each one with their unique instance IDs (e.g. 0 and 1) included in the `DefObject` structure.  You will then need to add a pointer to `M2MObject` to the constructor of each of your object classes and pass that pointer to this class.

For instance, the constructors might look as follows:

```
TemperatureOutdoor::TemperatureOutdoor(Callback<bool(Temperature *)> getCallback,
                                       M2MObject *object)
                   :M2MObjectHelper(&_defObject, NULL, object)
{
    ...
}

TemperatureIndoor::TemperatureIndoor(Callback<bool(Temperature *)> getCallback,
                                     M2MObject *object)
                  :M2MObjectHelper(&_defObject, NULL, object)
{
    ...
}
```

Then, in your application code, you would instantiate the two temperature objects as follows:

```
TemperatureOutdoor *outdoor = new TemperatureOutdoor(true,
                                                     getTemperatureDataOutdoor,
                                                     NULL);
M2MObject *objectOutdoor = getObject(outdoor);
TemperatureIndoor *indoor = new TemperatureIndoor(true,
                                                  getTemperatureDataIndoor,
                                                  objectOutdoor);
```

Clearing Up
-----------
When clearing objects up, always delete them BEFORE Mbed Client/Cloud Client itself is deleted; their destructors do things inside Mbed Client/Cloud Client.