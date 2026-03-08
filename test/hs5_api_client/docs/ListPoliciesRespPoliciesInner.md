# ListPoliciesRespPoliciesInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**name** | **str** |  | 
**document** | **str** |  | 
**system** | **bool** |  | 

## Example

```python
from hs5_api.models.list_policies_resp_policies_inner import ListPoliciesRespPoliciesInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListPoliciesRespPoliciesInner from a JSON string
list_policies_resp_policies_inner_instance = ListPoliciesRespPoliciesInner.from_json(json)
# print the JSON string representation of the object
print(ListPoliciesRespPoliciesInner.to_json())

# convert the object into a dict
list_policies_resp_policies_inner_dict = list_policies_resp_policies_inner_instance.to_dict()
# create an instance of ListPoliciesRespPoliciesInner from a dict
list_policies_resp_policies_inner_from_dict = ListPoliciesRespPoliciesInner.from_dict(list_policies_resp_policies_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


