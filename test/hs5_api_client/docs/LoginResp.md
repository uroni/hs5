# LoginResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**access_key** | **str** |  | 
**secret_access_key** | **str** |  | 

## Example

```python
from hs5_api.models.login_resp import LoginResp

# TODO update the JSON string below
json = "{}"
# create an instance of LoginResp from a JSON string
login_resp_instance = LoginResp.from_json(json)
# print the JSON string representation of the object
print(LoginResp.to_json())

# convert the object into a dict
login_resp_dict = login_resp_instance.to_dict()
# create an instance of LoginResp from a dict
login_resp_from_dict = LoginResp.from_dict(login_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


