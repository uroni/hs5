# ListAccessKeysResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**access_keys** | [**List[ListAccessKeysRespAccessKeysInner]**](ListAccessKeysRespAccessKeysInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_access_keys_resp import ListAccessKeysResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListAccessKeysResp from a JSON string
list_access_keys_resp_instance = ListAccessKeysResp.from_json(json)
# print the JSON string representation of the object
print(ListAccessKeysResp.to_json())

# convert the object into a dict
list_access_keys_resp_dict = list_access_keys_resp_instance.to_dict()
# create an instance of ListAccessKeysResp from a dict
list_access_keys_resp_from_dict = ListAccessKeysResp.from_dict(list_access_keys_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


