# ListRolesParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_roles_params import ListRolesParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolesParams from a JSON string
list_roles_params_instance = ListRolesParams.from_json(json)
# print the JSON string representation of the object
print(ListRolesParams.to_json())

# convert the object into a dict
list_roles_params_dict = list_roles_params_instance.to_dict()
# create an instance of ListRolesParams from a dict
list_roles_params_from_dict = ListRolesParams.from_dict(list_roles_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


