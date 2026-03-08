# HapiError


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**herror** | **str** |  | 
**msg** | **str** |  | 

## Example

```python
from hs5_api.models.hapi_error import HapiError

# TODO update the JSON string below
json = "{}"
# create an instance of HapiError from a JSON string
hapi_error_instance = HapiError.from_json(json)
# print the JSON string representation of the object
print(HapiError.to_json())

# convert the object into a dict
hapi_error_dict = hapi_error_instance.to_dict()
# create an instance of HapiError from a dict
hapi_error_from_dict = HapiError.from_dict(hapi_error_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


