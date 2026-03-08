# ListResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**objects** | [**List[ListRespObjectsInner]**](ListRespObjectsInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_resp import ListResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListResp from a JSON string
list_resp_instance = ListResp.from_json(json)
# print the JSON string representation of the object
print(ListResp.to_json())

# convert the object into a dict
list_resp_dict = list_resp_instance.to_dict()
# create an instance of ListResp from a dict
list_resp_from_dict = ListResp.from_dict(list_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


