# LoginParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**username** | **str** |  | 
**password** | **str** |  | 

## Example

```python
from hs5_api.models.login_params import LoginParams

# TODO update the JSON string below
json = "{}"
# create an instance of LoginParams from a JSON string
login_params_instance = LoginParams.from_json(json)
# print the JSON string representation of the object
print(LoginParams.to_json())

# convert the object into a dict
login_params_dict = login_params_instance.to_dict()
# create an instance of LoginParams from a dict
login_params_from_dict = LoginParams.from_dict(login_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


