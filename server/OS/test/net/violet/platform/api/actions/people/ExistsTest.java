package net.violet.platform.api.actions.people;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

import net.violet.platform.api.actions.AbstractTestBase;
import net.violet.platform.api.actions.Action;
import net.violet.platform.api.actions.ActionParam;
import net.violet.platform.api.callers.APICaller;
import net.violet.platform.api.callers.ApplicationAPICaller;
import net.violet.platform.api.exceptions.APIException;
import net.violet.platform.datamodel.Application;
import net.violet.platform.datamodel.ApplicationCredentials;
import net.violet.platform.datamodel.mock.ApplicationCredentialsMock;
import net.violet.platform.datamodel.mock.ApplicationMock;
import net.violet.platform.datamodel.mock.UserMock;
import net.violet.platform.dataobjects.ApplicationCredentialsData;

import org.junit.Assert;
import org.junit.Test;

public class ExistsTest extends AbstractTestBase {

	@Test
	public void existsTest() throws APIException {
		new UserMock(42, "myLogin", "myPassword", "myEmail@gmail.com", getSiteFrLang(), "France", "myFirstName", "myLastName", getParisTimezone(), "H", "Zip", "Paris", 1);

		final Date now = new Date();
		final Application theApplication = new ApplicationMock(42, "My first application", getPrivateUser(), now);;
		final ApplicationCredentials cred = new ApplicationCredentialsMock("6992873d28d86925325dc52d15d6feec30bb2da5", "59e6060a53ab1be5", theApplication);
		final APICaller caller = new ApplicationAPICaller(ApplicationCredentialsData.getData(cred));
		final Map<String, Object> theParams = new HashMap<String, Object>();

		theParams.put(ActionParam.MAIN_PARAM_KEY, "myEmail@gmail.com");
		final ActionParam theActionParam = new ActionParam(caller, theParams);
		final Action theAction = new Exists();
		final Object theResult = theAction.processRequest(theActionParam);

		Assert.assertTrue(theResult instanceof Boolean);
		Assert.assertTrue(new Boolean(theResult.toString()));
	}

	@Test
	public void unexistsTest() throws APIException {
		new UserMock(42, "myLogin", "myPassword", "myEmail@gmail.com", getSiteFrLang(), "France", "myFirstName", "myLastName", getParisTimezone(), "H", "Zip", "Paris", 1);

		final Date now = new Date();
		final Application theApplication = new ApplicationMock(42, "My first application", getPrivateUser(), now);;
		final ApplicationCredentials cred = new ApplicationCredentialsMock("6992873d28d86925325dc52d15d6feec30bb2da5", "59e6060a53ab1be5", theApplication);
		final APICaller caller = new ApplicationAPICaller(ApplicationCredentialsData.getData(cred));
		final Map<String, Object> theParams = new HashMap<String, Object>();

		theParams.put(ActionParam.MAIN_PARAM_KEY, "unexisting@gmail.com");
		final ActionParam theActionParam = new ActionParam(caller, theParams);
		final Action theAction = new Exists();
		final Object theResult = theAction.processRequest(theActionParam);

		Assert.assertTrue(theResult instanceof Boolean);
		Assert.assertFalse(new Boolean(theResult.toString()));
	}

}
