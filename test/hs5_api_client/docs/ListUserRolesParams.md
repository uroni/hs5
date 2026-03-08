# ListUserRolesParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**user_id** | **str** |  | 
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_user_roles_params import ListUserRolesParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListUserRolesParams from a JSON string
list_user_roles_params_instance = ListUserRolesParams.from_json(json)
# print the JSON string representation of the object
print(ListUserRolesParams.to_json())

# convert the object into a dict
list_user_roles_params_dict = list_user_roles_params_instance.to_dict()
# create an instance of ListUserRolesParams from a dict
list_user_roles_params_from_dict = ListUserRolesParams.from_dict(list_user_roles_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


