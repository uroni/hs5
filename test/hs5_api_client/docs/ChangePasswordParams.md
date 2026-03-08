# ChangePasswordParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**old_password** | **str** |  | 
**password** | **str** |  | 

## Example

```python
from hs5_api.models.change_password_params import ChangePasswordParams

# TODO update the JSON string below
json = "{}"
# create an instance of ChangePasswordParams from a JSON string
change_password_params_instance = ChangePasswordParams.from_json(json)
# print the JSON string representation of the object
print(ChangePasswordParams.to_json())

# convert the object into a dict
change_password_params_dict = change_password_params_instance.to_dict()
# create an instance of ChangePasswordParams from a dict
change_password_params_from_dict = ChangePasswordParams.from_dict(change_password_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


