# ListRespObjectsInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**type** | **int** |  | 
**name** | **str** |  | 
**size** | **int** |  | 
**last_modified** | **int** |  | 
**created** | **int** |  | 

## Example

```python
from hs5_api.models.list_resp_objects_inner import ListRespObjectsInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListRespObjectsInner from a JSON string
list_resp_objects_inner_instance = ListRespObjectsInner.from_json(json)
# print the JSON string representation of the object
print(ListRespObjectsInner.to_json())

# convert the object into a dict
list_resp_objects_inner_dict = list_resp_objects_inner_instance.to_dict()
# create an instance of ListRespObjectsInner from a dict
list_resp_objects_inner_from_dict = ListRespObjectsInner.from_dict(list_resp_objects_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


