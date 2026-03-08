# ListUsersRespUsersInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**username** | **str** |  | 
**id** | **str** |  | 
**system** | **bool** |  | 

## Example

```python
from hs5_api.models.list_users_resp_users_inner import ListUsersRespUsersInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListUsersRespUsersInner from a JSON string
list_users_resp_users_inner_instance = ListUsersRespUsersInner.from_json(json)
# print the JSON string representation of the object
print(ListUsersRespUsersInner.to_json())

# convert the object into a dict
list_users_resp_users_inner_dict = list_users_resp_users_inner_instance.to_dict()
# create an instance of ListUsersRespUsersInner from a dict
list_users_resp_users_inner_from_dict = ListUsersRespUsersInner.from_dict(list_users_resp_users_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


