# ListPoliciesResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**policies** | [**List[ListPoliciesRespPoliciesInner]**](ListPoliciesRespPoliciesInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_policies_resp import ListPoliciesResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListPoliciesResp from a JSON string
list_policies_resp_instance = ListPoliciesResp.from_json(json)
# print the JSON string representation of the object
print(ListPoliciesResp.to_json())

# convert the object into a dict
list_policies_resp_dict = list_policies_resp_instance.to_dict()
# create an instance of ListPoliciesResp from a dict
list_policies_resp_from_dict = ListPoliciesResp.from_dict(list_policies_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


