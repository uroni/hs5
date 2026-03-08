# AddUserRoleParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**user_id** | **str** |  | 
**role_id** | **str** |  | 

## Example

```python
from hs5_api.models.add_user_role_params import AddUserRoleParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddUserRoleParams from a JSON string
add_user_role_params_instance = AddUserRoleParams.from_json(json)
# print the JSON string representation of the object
print(AddUserRoleParams.to_json())

# convert the object into a dict
add_user_role_params_dict = add_user_role_params_instance.to_dict()
# create an instance of AddUserRoleParams from a dict
add_user_role_params_from_dict = AddUserRoleParams.from_dict(add_user_role_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


