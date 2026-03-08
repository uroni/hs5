# ListParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**path** | **str** |  | 
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_params import ListParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListParams from a JSON string
list_params_instance = ListParams.from_json(json)
# print the JSON string representation of the object
print(ListParams.to_json())

# convert the object into a dict
list_params_dict = list_params_instance.to_dict()
# create an instance of ListParams from a dict
list_params_from_dict = ListParams.from_dict(list_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


