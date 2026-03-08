# ListUsersParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_users_params import ListUsersParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListUsersParams from a JSON string
list_users_params_instance = ListUsersParams.from_json(json)
# print the JSON string representation of the object
print(ListUsersParams.to_json())

# convert the object into a dict
list_users_params_dict = list_users_params_instance.to_dict()
# create an instance of ListUsersParams from a dict
list_users_params_from_dict = ListUsersParams.from_dict(list_users_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


