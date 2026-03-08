# ListAccessKeysRespAccessKeysInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**access_key_id** | **str** |  | 

## Example

```python
from hs5_api.models.list_access_keys_resp_access_keys_inner import ListAccessKeysRespAccessKeysInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListAccessKeysRespAccessKeysInner from a JSON string
list_access_keys_resp_access_keys_inner_instance = ListAccessKeysRespAccessKeysInner.from_json(json)
# print the JSON string representation of the object
print(ListAccessKeysRespAccessKeysInner.to_json())

# convert the object into a dict
list_access_keys_resp_access_keys_inner_dict = list_access_keys_resp_access_keys_inner_instance.to_dict()
# create an instance of ListAccessKeysRespAccessKeysInner from a dict
list_access_keys_resp_access_keys_inner_from_dict = ListAccessKeysRespAccessKeysInner.from_dict(list_access_keys_resp_access_keys_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


