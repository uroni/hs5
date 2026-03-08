# LogoutParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**dummy** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.logout_params import LogoutParams

# TODO update the JSON string below
json = "{}"
# create an instance of LogoutParams from a JSON string
logout_params_instance = LogoutParams.from_json(json)
# print the JSON string representation of the object
print(LogoutParams.to_json())

# convert the object into a dict
logout_params_dict = logout_params_instance.to_dict()
# create an instance of LogoutParams from a dict
logout_params_from_dict = LogoutParams.from_dict(logout_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


