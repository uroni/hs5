# ListPoliciesParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_policies_params import ListPoliciesParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListPoliciesParams from a JSON string
list_policies_params_instance = ListPoliciesParams.from_json(json)
# print the JSON string representation of the object
print(ListPoliciesParams.to_json())

# convert the object into a dict
list_policies_params_dict = list_policies_params_instance.to_dict()
# create an instance of ListPoliciesParams from a dict
list_policies_params_from_dict = ListPoliciesParams.from_dict(list_policies_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


